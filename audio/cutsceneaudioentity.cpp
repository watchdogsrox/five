// 
// audio/cutsceneaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "cutsceneaudioentity.h"

#include "northaudioengine.h"
#include "dynamicmixer.h"
#include "radioaudioentity.h"
#include "scriptaudioentity.h"
#include "audioengine/engine.h"
#include "audiohardware/waveslot.h"
#include "audiosoundtypes/sounddefs.h"
#include "audiosoundtypes/streamingsound.h"
#include "audioengine/categorymanager.h"
#include "camera/CamInterface.h"
#include "cutscene/CutSceneManagerNew.h"
#include "scriptaudioentity.h"

#include "audioengine/controller.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audiohardware/syncsource.h"
#include "audiohardware/waveplayer.h"
#include "control/replay/audio/CutSceneAudioPacket.h"
#include "system/bootmgr.h"

#if RSG_DURANGO
#include "audiohardware/decoder_xbonexma.h"
#endif

#include "debugaudio.h"

AUDIO_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(Audio,CutsceneAudio)


namespace rage
{

	XPARAM(noaudio);
}

PARAM(allcutsceneaudio, "Allows the use of non-mastered cutscene audio");
PARAM(forcetrimmedcutscenes, "Force cutscene audio to use trimmed audio files (i.e. final disc versions)");

audCutsceneAudioEntity g_CutsceneAudioEntity;
bool g_CutsceneAudioPaused = false;
float g_CutsceneAudioCatchupFactor = 0.95f;
bool g_DidCutsceneJumpThisFrame = false;

bool g_GameTimerCutscenes = false;

PARAM(gametimercutscenes, "Cutscenes will be driven by the fwTimer - only use for debugging");

const char *g_CutsceneSoundNamePrefix = "CUTSCENES_";

BANK_ONLY(bool g_UseEditedCutscenes = true;)
BANK_ONLY(bool g_UseMasterCutscenes = true;)

audCutsceneAudioEntity::audCutsceneAudioEntity()
{

	for(u32 i=0; i<2; i++)
	{
		m_SoundNames[i][0] = '\0';
		m_SoundHashes[i] = 0;
		m_Sounds[i] = NULL;
		m_StreamSlots[i] = NULL;
		m_StartPrepareTime[i] = 0;
		m_SyncIds[i] = audMixerSyncManager::InvalidId;
		m_Scenes[i] = NULL;
		m_ScenePartialHashes[i] = 0;
		m_CutsceneAudioType[i] = AUD_CUTSCENE_TYPE_NONE;
		m_IsPrepared[i] = false;
		m_UsePlayPosition[i] = false;

#if __BANK
		m_SceneNames[i][0] = '\0';
#endif
	}

	m_NextWaveSlotIndex = 0;
}

void audCutsceneAudioEntity::Init(void)
{
	naAudioEntity::Init();
	audEntity::SetName("audCutsceneAudioEntity");
	m_PlayedThisFrame = false;
	m_LastStopTime = 0;
	m_WasFaded = false;
}

void audCutsceneAudioEntity::Shutdown(void)
{
	Stop(true);

	StopSyncScene();

	audEntity::Shutdown();
}

u32 audCutsceneAudioEntity::GetEngineTime()
{
	if(PARAM_noaudio.Get() || PARAM_gametimercutscenes.Get() || g_GameTimerCutscenes)
	{
		return fwTimer::GetTimeInMilliseconds();
	}
	return g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audCutsceneAudioEntity::SetVariableOnSound(u32 variableName, float value, s32 slotIndex)
{
	if(slotIndex == -1)
	{
		slotIndex = (m_NextWaveSlotIndex+1)%2;
	}

	if(m_Sounds[slotIndex])
	{
		m_Sounds[slotIndex]->FindAndSetVariableValue(variableName, value);
	}
}

bool audCutsceneAudioEntity::IsPrepared(s32 slot)
{
	if (PARAM_gametimercutscenes.Get() || g_GameTimerCutscenes)
	{
		return true;
	}

	if (slot >= 0)
	{
		return m_IsPrepared[slot];
	}
	return false;
}

audPrepareState audCutsceneAudioEntity::Prepare(audCutSceneEvent &cutEvent, const CutsceneAudioType type, s32 slotIndex)
{
	if(PARAM_gametimercutscenes.Get() || g_GameTimerCutscenes)
	{
		return AUD_PREPARED;
	}

	if(slotIndex == -1)
	{
		slotIndex = m_NextWaveSlotIndex;
	}

#if __BANK
	caDisplayf("Cutscene Audio Prepare (new): %s, type %d, offset %d, slot %d, time %d, FC: %u", cutEvent.TrackName, type, cutEvent.StartOffset, slotIndex, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
	if(m_IsPrepared[slotIndex])
	{
		caErrorf("Cutscene track is already prepared on entry to Prepare()");
	}
#endif

	u32 soundHash = atFinalizeHash(cutEvent.PartialHash);
	u32 editedSoundHash = 0, trimmedSoundHash = 0;
	u32 masteredSoundHash = 0, masteredOnlySoundHash = 0;
	REPLAY_ONLY(u32 masteredReplaySoundHash = 0, masteredOnlyReplaySoundHash = 0;)
	REPLAY_ONLY(u32 customReplaySoundHash = 0;)
	

	if(type ==  AUD_CUTSCENE_TYPE_CUTSCENE)
	{
		if(m_CutsceneTrack.GetScriptSuffix())
		{
			char cutsceneSuffix[128] = {0};
			formatf(cutsceneSuffix, "%s%s", m_CutsceneTrack.GetScriptSuffix(), "_EDITED");
			editedSoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "%s%s", m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_TRIMMED");
			trimmedSoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "%s%s", m_CutsceneTrack.GetScriptSuffix(), "_MASTERED");
			masteredSoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "%s%s", m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_ONLY");
			masteredOnlySoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
#if GTA_REPLAY
			formatf(cutsceneSuffix, "%s%s", m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_REPLAY");
			masteredReplaySoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "%s%s", m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_REPLAY_ONLY");
			masteredOnlyReplaySoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
#endif
		}
		else
		{
			char cutsceneSuffix[128] = {0};
			formatf(cutsceneSuffix, "%s", "_EDITED");
			editedSoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "%s", "_MASTERED_TRIMMED");
			trimmedSoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "%s", "_MASTERED");
			masteredSoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "%s", "_MASTERED_ONLY");
			masteredOnlySoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
#if GTA_REPLAY
			formatf(cutsceneSuffix, "_MASTERED_REPLAY");
			masteredReplaySoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
			formatf(cutsceneSuffix, "_MASTERED_REPLAY_ONLY");
			masteredOnlyReplaySoundHash = atStringHash(cutsceneSuffix, cutEvent.PartialHash);
#endif
		}
	}
	else if(type == AUD_CUTSCENE_TYPE_SYNCHED_SCENE)
	{
		editedSoundHash = atStringHash("_CUSTOM", cutEvent.PartialHash);
		masteredSoundHash = atStringHash("_SYNC_MASTERED", cutEvent.PartialHash);
		trimmedSoundHash = atStringHash("_SYNC_MASTERED_TRIMMED", cutEvent.PartialHash);
		masteredOnlySoundHash = atStringHash("_SYNC_MASTERED_ONLY", cutEvent.PartialHash); 
#if GTA_REPLAY
		masteredReplaySoundHash = atStringHash("_SYNC_MASTERED_REPLAY", cutEvent.PartialHash);
		masteredOnlyReplaySoundHash = atStringHash("_SYNC_MASTERED_REPLAY_ONLY", cutEvent.PartialHash);
#endif
	}

	REPLAY_ONLY(customReplaySoundHash = atStringHash("_CUSTOM_REPLAY", cutEvent.PartialHash);)


#if 1 //__BANK
	char masteredSoundName[64] = {0};
	char editedSoundName[64] = {0};
	REPLAY_ONLY(char customReplaySoundName[64] = {0};)
	char masteredOnlySoundName[64] = {0};
	char trimmedSoundName[64] = {0};
#if GTA_REPLAY
	char masteredReplaySoundName[64] = {0};
	char masteredOnlyReplaySoundName[64] = {0};
#endif

	REPLAY_ONLY(formatf(customReplaySoundName, "%s%s", cutEvent.TrackName, "_CUSTOM_REPLAY");)

	if(m_CutsceneTrack.GetScriptSuffix() &&  type == AUD_CUTSCENE_TYPE_CUTSCENE)
	{
		formatf(masteredSoundName, "%s%s%s", cutEvent.TrackName, m_CutsceneTrack.GetScriptSuffix(), "_MASTERED");
		REPLAY_ONLY(formatf(masteredReplaySoundName, "%s%s%s", cutEvent.TrackName, m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_REPLAY");)
		formatf(editedSoundName, "%s%s%s", cutEvent.TrackName, m_CutsceneTrack.GetScriptSuffix(), "_EDITED");
		formatf(trimmedSoundName, "%s%s%s", cutEvent.TrackName, m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_TRIMMED");
		formatf(masteredOnlySoundName, "%s%s%s", cutEvent.TrackName, m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_ONLY");
		REPLAY_ONLY(formatf(masteredOnlyReplaySoundName, "%s%s%s", cutEvent.TrackName, m_CutsceneTrack.GetScriptSuffix(), "_MASTERED_REPLAY_ONLY");)
	}
	else
	{
		formatf(masteredSoundName, "%s%s", cutEvent.TrackName, cutEvent.TrackName, type == AUD_CUTSCENE_TYPE_CUTSCENE?  "_MASTERED":"_SYNC_MASTERED");
		REPLAY_ONLY(formatf(masteredReplaySoundName, "%s%s", cutEvent.TrackName, cutEvent.TrackName, type == AUD_CUTSCENE_TYPE_CUTSCENE?  "_MASTERED_REPLAY":"_SYNC_MASTERED_REPLAY");)
		formatf(editedSoundName, "%s%s", cutEvent.TrackName, type == AUD_CUTSCENE_TYPE_CUTSCENE? "_EDITED": "_CUSTOM");
		formatf(trimmedSoundName, "%s%s", cutEvent.TrackName, type == AUD_CUTSCENE_TYPE_CUTSCENE? "_MASTERED_TRIMMED" : "_SYNC_MASTERED_TRIMMED");
		formatf(masteredOnlySoundName, "%s%s", cutEvent.TrackName, type == AUD_CUTSCENE_TYPE_CUTSCENE? "_MASTERED_ONLY" : "_SYNC_MASTERED_ONLY"); 
		REPLAY_ONLY(formatf(masteredOnlyReplaySoundName, "%s%s", cutEvent.TrackName, type == AUD_CUTSCENE_TYPE_CUTSCENE? "_MASTERED_REPLAY_ONLY" : "_SYNC_MASTERED_REPLAY_ONLY");)
	}
#endif


	if(caVerifyf(m_CutsceneAudioType[slotIndex] == AUD_CUTSCENE_TYPE_NONE || type == m_CutsceneAudioType[slotIndex], "Trying to prepare a cutscene with type %d but a cutscene of type %d is already being prepared. Speak to Animation code", type, m_CutsceneAudioType[slotIndex]))
	{
		m_CutsceneAudioType[slotIndex] = type;
	}
	else
	{
		return AUD_PREPARE_FAILED;
	}

	if((m_SoundHashes[slotIndex] != soundHash) && (m_SoundHashes[slotIndex] != editedSoundHash) && (m_SoundHashes[slotIndex] != masteredSoundHash) && (m_SoundHashes[slotIndex] != trimmedSoundHash) && (m_SoundHashes[slotIndex] != masteredOnlySoundHash) REPLAY_ONLY( && (m_SoundHashes[slotIndex] != masteredReplaySoundHash) && (m_SoundHashes[slotIndex] != masteredOnlyReplaySoundHash) && (m_SoundHashes[slotIndex] != customReplaySoundHash)) )
	{
		//We've not yet created this sound. 
		if(m_Sounds[slotIndex])
		{
			BANK_ONLY(caAssertf(0, "Preparing cutscene/synched scene audio %s on a slot that already has a sound %s", cutEvent.TrackName, m_SoundNames[slotIndex]));
			m_Sounds[slotIndex]->StopAndForget();

			StopAndForgetSounds(m_StereoShadowSounds[slotIndex].Left, m_StereoShadowSounds[slotIndex].Right);
		}

		audSoundInitParams initParams;
		initParams.StartOffset = cutEvent.StartOffset;
		Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
		initParams.RemoveHierarchy = false;

		audEntity * playEntity = NULL;

		if(type != AUD_CUTSCENE_TYPE_CUTSCENE)
		{
			int sceneIndex = m_SynchedSceneTrack.GetEventIndexFromHash(atFinalizeHash(cutEvent.ScenePartialHash));

			if(m_SynchedSceneTrack.UseEntity(sceneIndex) && m_SynchedSceneTrack.GetPlayEntity(sceneIndex))
			{
				playEntity = m_SynchedSceneTrack.GetPlayEntity(sceneIndex)->GetAudioEntity();
				initParams.TrackEntityPosition = true;
			}

			if(m_SynchedSceneTrack.UsePosition(sceneIndex))
			{
				initParams.Position = VEC3V_TO_VECTOR3(m_SynchedSceneTrack.GetPlayPosition(sceneIndex));
			}

			if(!playEntity && !m_SynchedSceneTrack.UsePosition(sceneIndex))
			{
				Sound::tSpeakerMask speakerMask; 
				speakerMask.Value = 0; 
				speakerMask.BitFields.FrontCenter = true;
				initParams.SpeakerMask = speakerMask.Value;
			}
		}

		if(!playEntity)
		{
			playEntity = (audEntity*)this;
		}

		if(type == AUD_CUTSCENE_TYPE_CUTSCENE)
		{
			if(!m_Sounds[slotIndex] && !PARAM_forcetrimmedcutscenes.Get())
			{
#if GTA_REPLAY
				if(CReplayMgr::IsReplayInControlOfWorld())
				{
					playEntity->CreateSound_PersistentReference(masteredOnlyReplaySoundHash, &m_Sounds[slotIndex], &initParams);
					m_SoundHashes[slotIndex] = masteredOnlyReplaySoundHash;
					strcpy(m_SoundNames[slotIndex], masteredOnlyReplaySoundName);
					if(!PARAM_allcutsceneaudio.Get())
					{
						caAssertf(m_Sounds[slotIndex], "Couldn't get mastered_only_replay cutscene audio %s", masteredOnlyReplaySoundName);
					}
				}
#endif
				if(!m_Sounds[slotIndex])
				{
					playEntity->CreateSound_PersistentReference(masteredOnlySoundHash, &m_Sounds[slotIndex], &initParams);
					m_SoundHashes[slotIndex] = masteredOnlySoundHash;
					strcpy(m_SoundNames[slotIndex], masteredOnlySoundName);
					if(!PARAM_allcutsceneaudio.Get())
					{
						caAssertf(m_Sounds[slotIndex], "Couldn't get mastered_only cutscene audio %s", masteredOnlySoundName);
					}
				}
			}

			if(!m_Sounds[slotIndex] BANK_ONLY(&& g_UseMasterCutscenes))
			{
				playEntity->CreateSound_PersistentReference(masteredSoundHash, &m_Sounds[slotIndex], &initParams);
				m_SoundHashes[slotIndex] = masteredSoundHash;
				strcpy(m_SoundNames[slotIndex], masteredSoundName);
			}

			if(!PARAM_forcetrimmedcutscenes.Get() && PARAM_allcutsceneaudio.Get())
			{
#if GTA_REPLAY
				if(CReplayMgr::IsReplayInControlOfWorld())
				{
					playEntity->CreateSound_PersistentReference(masteredReplaySoundHash, &m_Sounds[slotIndex], &initParams);
					m_SoundHashes[slotIndex] = masteredReplaySoundHash;
					strcpy(m_SoundNames[slotIndex], masteredReplaySoundName);
					if(!PARAM_allcutsceneaudio.Get())
					{
						caAssertf(m_Sounds[slotIndex], "Couldn't get mastered_replayy cutscene audio %s", masteredOnlyReplaySoundName);
					}
				}
#endif
				if(!m_Sounds[slotIndex] BANK_ONLY(&& g_UseEditedCutscenes))
				{
					playEntity->CreateSound_PersistentReference(editedSoundHash, &m_Sounds[slotIndex], &initParams);
					m_SoundHashes[slotIndex] = editedSoundHash;
					strcpy(m_SoundNames[slotIndex], editedSoundName);
				}
				if(!m_Sounds[slotIndex])
				{
					playEntity->CreateSound_PersistentReference(soundHash, &m_Sounds[slotIndex], &initParams);
					m_SoundHashes[slotIndex] = soundHash;
					strcpy(m_SoundNames[slotIndex], cutEvent.TrackName);
				}
			
			}

			if(!m_Sounds[slotIndex] && PARAM_forcetrimmedcutscenes.Get())
			{
				playEntity->CreateSound_PersistentReference(trimmedSoundHash, &m_Sounds[slotIndex], &initParams);
				m_SoundHashes[slotIndex] = trimmedSoundHash;
				strcpy(m_SoundNames[slotIndex], trimmedSoundName);
				if(/*sysBootManager::IsBootedFromDisc() || RSG_FINAL || */PARAM_forcetrimmedcutscenes.Get())
				{
					caAssertf(m_Sounds[slotIndex], "Couldn't get trimmed cutscene audio %s", trimmedSoundName);
				}
				if(m_Sounds[slotIndex]) //remove any audio offset that's snuck in
				{
					cutEvent.StartOffset-=cutEvent.AudioOffset;
					cutEvent.AudioOffset = 0;
				}
			}
		}
		else
		{

#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				playEntity->CreateSound_PersistentReference(customReplaySoundHash, &m_Sounds[slotIndex], &initParams);
				m_SoundHashes[slotIndex] = customReplaySoundHash;
				strcpy(m_SoundNames[slotIndex], customReplaySoundName);
				if(!PARAM_allcutsceneaudio.Get())
				{
					caAssertf(m_Sounds[slotIndex], "Couldn't get custom_replay cutscene audio %s", masteredOnlyReplaySoundName);
				}
				
				if(!m_Sounds[slotIndex])
				{
					playEntity->CreateSound_PersistentReference(masteredOnlyReplaySoundHash, &m_Sounds[slotIndex], &initParams);
					m_SoundHashes[slotIndex] = masteredOnlyReplaySoundHash;
					strcpy(m_SoundNames[slotIndex], masteredOnlyReplaySoundName);
					if(!PARAM_allcutsceneaudio.Get())
					{
						caAssertf(m_Sounds[slotIndex], "Couldn't get mastered_only_replay cutscene audio %s", masteredOnlyReplaySoundName);
					}
				}
			}
#endif

			if(!m_Sounds[slotIndex] BANK_ONLY(&& g_UseEditedCutscenes))
			{
				playEntity->CreateSound_PersistentReference(editedSoundHash, &m_Sounds[slotIndex], &initParams);
				m_SoundHashes[slotIndex] = editedSoundHash;
				strcpy(m_SoundNames[slotIndex], editedSoundName);
			}


			if(!m_Sounds[slotIndex])
			{
				playEntity->CreateSound_PersistentReference(masteredOnlySoundHash, &m_Sounds[slotIndex], &initParams);
				m_SoundHashes[slotIndex] = masteredOnlySoundHash;
				strcpy(m_SoundNames[slotIndex], masteredOnlySoundName);
				caAssertf(m_Sounds[slotIndex], "Couldn't get mastered_only synched scene audio %s", masteredOnlySoundName);
			}	
		}

#if __BANK
		formatf(m_SceneNames[slotIndex], "%s_SCENE", cutEvent.SceneName);
#endif
	}

	//Default to prepared so we don't go into an infinite loop with missing audio tracks.
	audPrepareState prepareState = AUD_PREPARED;

	if(m_Sounds[slotIndex])
	{
		if(m_StreamSlots[slotIndex] == NULL)
		{
			audStreamClientSettings settings; 
			settings.priority = audStreamSlot::STREAM_PRIORITY_CUTSCENE;
			settings.stopCallback = &OnStopCallback;
			settings.hasStoppedCallback = &HasStoppedCallback;
			settings.userData = slotIndex;

			m_StreamSlots[slotIndex] = audStreamSlot::AllocateSlot(&settings);

			m_StartPrepareTime[slotIndex] = fwTimer::GetTimeInMilliseconds();

			caAssertf(m_StreamSlots[slotIndex], "Couldn't allocate streaming audio slot for cutscene");
			if(!m_StreamSlots[slotIndex])
			{
				caErrorf("Couldn't allocate streaming audio slot for cutscene");
			}
		}

		static const u32 k_MaxPrepareTime = 20000;

		if(m_StreamSlots[slotIndex] && (m_StreamSlots[slotIndex]->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE))
		{
			prepareState = m_Sounds[slotIndex]->Prepare(m_StreamSlots[slotIndex]->GetWaveSlot(), true);

			u32 prepareTime = fwTimer::GetTimeInMilliseconds() - m_StartPrepareTime[slotIndex];
			u32 numReferences = audWaveSlot::GetSlotReferenceCount(m_StreamSlots[slotIndex]->GetWaveSlot()->GetSlotIndex());
			if(numReferences > 0)
			{
				caDisplayf("Preparing cutscene audio on a waveslot with %u references. We've been preparing for %u ms, prepared state %u ", numReferences, prepareTime, prepareState);
				if(prepareState != AUD_PREPARED && prepareTime > k_MaxPrepareTime)
				{
					caErrorf("Looks like our cutscene audio prepare took too long; bailing out");
#if __BANK
					caErrorf("=========== Sound pool:");
					audSound::GetStaticPool().DebugPrintSoundPool();
					caErrorf("=========== Physical voices:");
					audDriver::GetVoiceManager().PrintPhysicalVoices();
#if RSG_DURANGO
					caErrorf("=========== Xma stats:");
					audDecoderXboxOneXma::PrintDecoderStats();
					caErrorf("=========== Xma contexts:");
					audDecoderXboxOneXma::PrintUsedXmaContexts();

#endif //RSG_DURANGO
					caErrorf("===========");
#endif //BANK
					m_Sounds[slotIndex]->StopAndForget();
					prepareState = AUD_PREPARE_FAILED;
				}
			}
		}
		else
		{
			prepareState = AUD_PREPARING; //we need to wait for a stream slot to become available.
		}


		if(prepareState == AUD_PREPARED && m_SyncIds[slotIndex] == audMixerSyncManager::InvalidId)
		{
			m_SyncIds[slotIndex] = audMixerSyncManager::Get()->Allocate();

			if(m_SyncIds[slotIndex] != audMixerSyncManager::InvalidId)   
			{ 
				caAssertf(!m_Scenes[slotIndex], "Cutscene audio Scene for slot %d is still active in Prpare", slotIndex);
				m_ScenePartialHashes[slotIndex] = cutEvent.ScenePartialHash;
			
				audMixerSyncManager::Get()->AddTriggerRef(m_SyncIds[slotIndex]);

				m_Sounds[slotIndex]->SetRequestedSyncSlaveId(m_SyncIds[slotIndex]);
				//snaErrorf("Setting slave sync id %u", m_SyncIds[slotIndex]);

				//This was asserting but may already be fixed: try with it again once the big cutscene push is done.
				//if(m_CutsceneTrack.GetCurrentPlayIndex() > -1)
				//{
				//	//m_Sounds[m_CutsceneTrack.GetCurrentPlayIndex()]->SetRequestedSyncMasterId(m_SyncIds[slotIndex]);
				//	u32 wavePlayerId = ((audStreamingSound*)m_Sounds[m_CutsceneTrack.GetCurrentPlayIndex()])->GetFirstWavePlayerId();
				//	audPcmSourceInterface::SetParam(wavePlayerId, audPcmSource::Params::SyncMasterId, m_SyncIds[slotIndex]);
				//	
				//	u32 triggerTimeMs = cutEvent.TriggerTime + m_CutsceneTrack.GetEventFromIndex(0).StartOffset - m_CutsceneTrack.GetEventFromIndex(0).TriggerTime;
				//	u32 triggerTimeSamples = audDriverUtil::ConvertMsToSamples(triggerTimeMs, 32000);
				//	audPcmSourceInterface::SetParam(wavePlayerId, audWavePlayer::Params::SyncTriggerTimeSamples, triggerTimeSamples);
				//}
				//else
				//{
				//	audMixerSyncManager::Get()->AddTriggerRef(m_SyncIds[slotIndex]);
				//}

				// This will prep the decoder/VRAM fetch so that we can kick off playback 'instantly'
				// Voices won't actually start playing until we trigger the sync id
				m_Sounds[slotIndex]->Play();


				// Look for a multitrack reverb sound
				Sound uncompressedSound;
				const u32 reverbSoundName = atStringHash("_REV", m_ScenePartialHashes[slotIndex]);
				const MultitrackSound *multitrackSound = g_AudioEngine.GetSoundManager().GetFactory().DecompressMetadata<MultitrackSound>(reverbSoundName, uncompressedSound);

				if(multitrackSound)
				{					
					caAssertf(multitrackSound->numSoundRefs == 2, "Cutscene reverb multitrack expected to have 2 sounds; this one has %u", multitrackSound->numSoundRefs);
					audSoundInitParams initParams;
					initParams.ShadowPcmSourceId = ((audStreamingSound*)m_Sounds[slotIndex])->GetFirstWavePlayerId();

					CreateAndPlaySound_Persistent(multitrackSound->SoundRef[0].SoundId, &m_StereoShadowSounds[slotIndex].Left, &initParams);
					CreateAndPlaySound_Persistent(multitrackSound->SoundRef[1].SoundId, &m_StereoShadowSounds[slotIndex].Right, &initParams);

					caDisplayf("Created stereo shadow sounds for cutscene");
					caAssert(m_StereoShadowSounds[slotIndex].Left);
					caAssert(m_StereoShadowSounds[slotIndex].Right);

					if(m_StereoShadowSounds[slotIndex].Left)
					{
						audEnvironmentGameMetric &metric = m_StereoShadowSounds[slotIndex].Left->GetRequestedSettings()->GetEnvironmentGameMetric();
						metric.SetJustReverb(true);
						metric.SetReverbSmall(0.f);
						metric.SetReverbMedium(0.f);
						metric.SetReverbLarge(0.f);						
					}
					if(m_StereoShadowSounds[slotIndex].Right)
					{						
						audEnvironmentGameMetric &metric = m_StereoShadowSounds[slotIndex].Right->GetRequestedSettings()->GetEnvironmentGameMetric();
						metric.SetJustReverb(true);
						metric.SetReverbSmall(0.f);
						metric.SetReverbMedium(0.f);
						metric.SetReverbLarge(0.f);
					}
				}

				//We need to spin round one more time to make sure all our voices are ready to trigger

				prepareState = AUD_PREPARING;
			}
			else
			{
				audWarningf("Failed to allocate mixer sync source for cutscene; will retry next frame");
				prepareState = AUD_PREPARING;
			}
		}
		else if (prepareState == AUD_PREPARED)
		{

			if((audMixerSyncManager::Get()->GetTriggerRefCount(m_SyncIds[slotIndex]) > 1)) 
			{
				prepareState = AUD_PREPARING;
			}

			//Check to see if we've taken too long and need to bail out
			u32 prepareTime = fwTimer::GetTimeInMilliseconds() - m_StartPrepareTime[slotIndex];
			caDisplayf("Waiting for cutscene audio sync. We've been preparing for %u ms, prepared state %u", prepareTime, prepareState);
			if(prepareState != AUD_PREPARED && prepareTime > k_MaxPrepareTime)
			{
				caErrorf("Looks like our cutscene audio sync took too long; bailing out");
#if __BANK
				caErrorf("=========== Sound pool:");
				audSound::GetStaticPool().DebugPrintSoundPool();
				caErrorf("=========== Physical voices:");
				audDriver::GetVoiceManager().PrintPhysicalVoices();
#if RSG_DURANGO
				caErrorf("=========== Xma stats:");
				audDecoderXboxOneXma::PrintDecoderStats();
				caErrorf("=========== Xma contexts:");
				audDecoderXboxOneXma::PrintUsedXmaContexts();

#endif //RSG_DURANGO
				caErrorf("===========");
#endif //BANK
				m_Sounds[slotIndex]->StopAndForget();
				prepareState = AUD_PREPARE_FAILED;
			}
		}
	}
	else
	{
		caWarningf("No audio track for cutscene: %s", cutEvent.TrackName);
	}

	if(prepareState != AUD_PREPARING)
	{
		m_IsPrepared[slotIndex] = true;
	}

	return prepareState;
}


void audCutsceneAudioEntity::Play(s32 slotIndex)
{
	if(slotIndex == -1)
	{
		slotIndex = m_NextWaveSlotIndex;
	}

#if __BANK
	caDisplayf("Cutscene Audio Play: %s, type %d, slot %d, sound %p, time %d, FC: %u", m_SoundNames[slotIndex], m_CutsceneAudioType[slotIndex], slotIndex, m_Sounds[slotIndex], fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif

	if(m_Sounds[slotIndex]) 
	{
		if(caVerifyf(m_StreamSlots[slotIndex], "Stream slot is null when trying to Play cutscene"))
		{
			// The sound should already be playing, but the voices will be in a waiting-for-sync state
			caAssertf(m_Sounds[slotIndex]->GetPlayState() == AUD_SOUND_PLAYING, "Playstate = %d", m_Sounds[slotIndex]->GetPlayState());

			audMetadataObjectInfo info;
			
#if __BANK
			caDisplayf("Trying to start cutscene dynamic mix scene %s", m_SceneNames[slotIndex]);
#endif

			const u32 sceneHash = atStringHash("_SCENE", m_ScenePartialHashes[slotIndex]);
			if(DYNAMICMIXMGR.GetMetadataManager().GetObjectInfo(sceneHash, info))
			{
				DYNAMICMIXER.StartScene(sceneHash, &m_Scenes[slotIndex]);
			}
			else if(m_CutsceneAudioType[slotIndex] != AUD_CUTSCENE_TYPE_CUTSCENE)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("SYNCHED_SCENE_DEFAULT_SCENE", 0xE69FFD02), &m_Scenes[slotIndex]);
			}
			else
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("CUTSCENE_DEFAULT_SCENE", 0x3E96C108), &m_Scenes[slotIndex]);
			}

			if(m_CutsceneAudioType[slotIndex] == AUD_CUTSCENE_TYPE_CUTSCENE && m_Scenes[slotIndex] && m_Scenes[slotIndex]->GetSceneSettings() && AUD_GET_TRISTATE_VALUE(m_Scenes[slotIndex]->GetSceneSettings()->Flags, FLAG_ID_MIXERSCENE_CUSCENEAUDIOOVERRUNS) == AUD_TRISTATE_TRUE)
			{
				m_CutsceneTrack.SetCanOverrun(true);
			}

			if(m_SyncIds[slotIndex] != audMixerSyncManager::InvalidId)
			{
				audMixerSyncManager::Get()->Trigger(m_SyncIds[slotIndex], 0);
			}


			m_NeedToPreloadRadio = true;
			m_PlayedThisFrame = true;

			m_IsPrepared[slotIndex] = false;

			//We are now playing a stream from this wave slot, so toggle the wave slot to be used to preload the next cutscene.
			// - This enables overlapping loads with playback for split cutscenes.
			m_NextWaveSlotIndex = (slotIndex + 1) % 2; 
		}
	}
}

void audCutsceneAudioEntity::PreUpdateService(u32 UNUSED_PARAM(timeInMs))
{
	g_DidCutsceneJumpThisFrame = false;
	m_CutsceneTrack.Update();
	m_SynchedSceneTrack.Update();

	bool played0 = (m_Sounds[0] && m_Sounds[0]->GetPlayState() == AUD_SOUND_PLAYING) || m_PlayedThisFrame;
	bool played1 = (m_Sounds[1] && m_Sounds[1]->GetPlayState() == AUD_SOUND_PLAYING) || m_PlayedThisFrame;
	bool isPlaying = ((CutSceneManager::GetInstance()&&!CutSceneManager::GetInstance()->IsStreamedCutScene()) && (played0 || played1));
	bool wasPlayingRecently = (fwTimer::GetTimeInMilliseconds() < m_LastStopTime + 500);
	//Only mute non-cutscene audio when we're playing a non-ped-interaction cutscene, i.e a traditional screen-faded mocap cutscene.
	if(isPlaying || wasPlayingRecently NA_RADIO_ENABLED_ONLY(|| audRadioStation::IsPlayingEndCredits()))
	{
		NA_RADIO_ENABLED_ONLY(g_RadioAudioEntity.MuteRetunes());
	}
	else
	{
		NA_RADIO_ENABLED_ONLY(g_RadioAudioEntity.UnmuteRetunes());
	}

	for(int i=0; i<2; i++)
	{
		if(!m_Sounds[i]) 
		{
			if(m_SyncIds[i] != audMixerSyncManager::InvalidId)
			{
				if(audMixerSyncManager::Get()->GetTriggerRefCount(m_SyncIds[i]))
				{
					audMixerSyncManager::Get()->Cancel(m_SyncIds[i]);
				}
				audMixerSyncManager::Get()->Release(m_SyncIds[i]);
				m_SyncIds[i] = audMixerSyncManager::InvalidId;
			}

			if(m_Scenes[i])
			{
				m_Scenes[i]->Stop();
			}

			if(m_StreamSlots[i])
			{ 
				//We have finished with our stream slot, so free it.
				m_StreamSlots[i]->Free();
				m_StreamSlots[i] = NULL;
			}

			m_StartPrepareTime[i] = 0;

			m_SoundNames[i][0] = '\0';
			m_SoundHashes[i] = 0;

#if __BANK
			m_SceneNames[i][0] = '\0';
#endif

			m_CutsceneAudioType[i] = AUD_CUTSCENE_TYPE_NONE;
		}
		else
		{
			f32 fade = 1.f - camInterface::GetFadeLevel();
			if(fade < 1.f && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowCutsceneOverScreenFade))
			{
				m_Sounds[i]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(fade));
				m_WasFaded = true;
			}
			else if(m_WasFaded)
			{
				m_Sounds[i]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(1.f));
				m_WasFaded = false;
			}
		}
	}

	m_PlayedThisFrame = false;
}

void audCutsceneAudioEntity::PostUpdate()
{
	m_SynchedSceneTrack.ClearUnusedEntities();
}

void audCutsceneAudioEntity::Stop(bool shouldStopAllStreams, s32 slotIndex, s32 release)
{
	if(slotIndex == -1)
	{
		slotIndex = (m_NextWaveSlotIndex+1)%2;
	}

#if __BANK
	caDisplayf("Cutscene Audio Stop: %s, type %d, slot %d, shouldStopAllStreams %s, time %d, FC: %u", m_SoundNames[slotIndex], m_CutsceneAudioType[slotIndex], slotIndex, shouldStopAllStreams?"true":"false", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif

	u8 numStreamsToStop;

	if(shouldStopAllStreams)
	{
		numStreamsToStop = 2;
	}
	else
	{
		numStreamsToStop = 1;
	}

	u32 waveSlotIndex = slotIndex;

	for(u32 streamIndex=0; streamIndex<numStreamsToStop; streamIndex++)
	{
		m_IsPrepared[waveSlotIndex] = false;

		if(m_Scenes[waveSlotIndex])
		{
			m_Scenes[waveSlotIndex]->Stop();
		}

		if(m_Sounds[waveSlotIndex])
		{
			m_LastStopTime = fwTimer::GetTimeInMilliseconds();
			m_Sounds[waveSlotIndex]->SetReleaseTime(release);
			m_Sounds[waveSlotIndex]->StopAndForget();
			m_Sounds[waveSlotIndex] = NULL;
		}

		StopAndForgetSounds(m_StereoShadowSounds[waveSlotIndex].Left, m_StereoShadowSounds[waveSlotIndex].Right);

		m_SoundNames[waveSlotIndex][0] = '\0';
		m_SoundHashes[waveSlotIndex] = 0;

#if __BANK
		m_SceneNames[waveSlotIndex][0] = '\0';
#endif

		if(m_SyncIds[waveSlotIndex] != audMixerSyncManager::InvalidId)
		{
			if(audMixerSyncManager::Get()->GetTriggerRefCount(m_SyncIds[waveSlotIndex]))
			{
				audMixerSyncManager::Get()->Cancel(m_SyncIds[waveSlotIndex]);
			}
			audMixerSyncManager::Get()->Release(m_SyncIds[waveSlotIndex]);
			m_SyncIds[waveSlotIndex] = audMixerSyncManager::InvalidId;
		}

		m_CutsceneAudioType[waveSlotIndex] = AUD_CUTSCENE_TYPE_NONE;

		waveSlotIndex = (waveSlotIndex + 1) % 2;
	}

	for(u32 loop = 0; loop < 2; loop++)
	{
		if(m_StreamSlots[loop] && m_Sounds[loop] == NULL)
		{ 
			//We have finished with our stream slot, so free it.
			m_StreamSlots[loop]->Free();
			m_StreamSlots[loop] = NULL;
		}
		m_StartPrepareTime[loop] = 0;
	}
}

void audCutsceneAudioEntity::Pause()
{
	g_CutsceneAudioPaused = true;
}

void audCutsceneAudioEntity::UnPause() 
{
	g_CutsceneAudioPaused = false; 
}

s32 g_CutsceneTimeOffset =0;
float g_CutsceneTimeScale = 1.f;

s32 audCutsceneAudioEntity::GetPlayTimeMs(s32 slotIndex) 
{
	if(slotIndex == -1)
	{
		slotIndex = (m_NextWaveSlotIndex+1)%2;
	}

	s32 playTimeMs = -1;
	u8 playingWaveSlotIndex = (u8)slotIndex;

	if(m_Sounds[playingWaveSlotIndex])
	{
		playTimeMs = ((audStreamingSound*)m_Sounds[playingWaveSlotIndex])->GetCurrentPlayTimeOfWave();

		if(playTimeMs > 0)
		{
			playTimeMs -= ((audStreamingSound*)m_Sounds[playingWaveSlotIndex])->GetStartOffset();
		}
	}
	else
	{
		audErrorf("Trying to get play time of a sound that is null");
		return playTimeMs;
	}

	if(playTimeMs > ComputeSoundDuration())
	{
		return -1;
	}

#if !__FINAL
	playTimeMs = (u32)((float)playTimeMs * g_CutsceneTimeScale);
	playTimeMs += g_CutsceneTimeOffset;
#endif

	return playTimeMs; 
}

s32 audCutsceneAudioEntity::GetPlayTimeIncludingOffsetMs(s32 slotIndex) 
{
	s32 playTimeMs = -1;
	u8 playingWaveSlotIndex = (u8)slotIndex;

	if(m_Sounds[playingWaveSlotIndex])
	{
		playTimeMs = ((audStreamingSound*)m_Sounds[playingWaveSlotIndex])->GetCurrentPlayTimeOfWave();
	}
	else
	{
		caErrorf("Trying to get play time of a sound that is null");
		return playTimeMs;
	}

	return playTimeMs; 
}

s32 audCutsceneAudioEntity::ComputeSoundDuration(s32 slotIndex)
{
	if(slotIndex == -1)
	{
		slotIndex = (m_NextWaveSlotIndex+1)%2;
	}

	s32 duration = -1;

	if(caVerifyf(m_Sounds[slotIndex], "Trying to get play time of a sound that is null"))
	{
		duration = ((audStreamingSound*)m_Sounds[slotIndex])->ComputeDurationMsIncludingStartOffsetAndPredelay(NULL);
	}

	return duration;
}

bool audCutsceneAudioEntity::OnStopCallback(u32 UNUSED_PARAM(userData))
{
	//We are losing our stream slot, so stop the cutscene stream.
	g_CutsceneAudioEntity.Stop(true);

	return true;
}

bool audCutsceneAudioEntity::HasStoppedCallback(u32 userData)
{
	bool hasStopped = true;
	
	//Check if we are still loading or playing from our allocated wave slots.
	//for(u8 waveSlotIndex=0; waveSlotIndex<2; waveSlotIndex++)
	{
		audStreamSlot *streamSlot = g_CutsceneAudioEntity.GetStreamSlot(userData);
		if(streamSlot)
		{
			audWaveSlot *waveSlot = streamSlot->GetWaveSlot();
			if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
			{
				hasStopped = false;
			}
		}
	}

	return hasStopped;
}

void audCutsceneAudioEntity::InitCutsceneTrack(u32 skipTimeInMs)
{
	m_CutsceneTrack.Init(skipTimeInMs);
}

bool audCutsceneAudioEntity::IsSyncSceneTrackPrepared(const char * audioName)
{
	return m_SynchedSceneTrack.IsPrepared(audioName);
}

bool audCutsceneAudioEntity::IsCutsceneTrackPrepared()
{
	return m_CutsceneTrack.IsPrepared();
}

bool audCutsceneAudioEntity::IsCutsceneTrackActive()
{
	return m_CutsceneTrack.IsActive();
}

void audCutsceneAudioEntity::TriggerCutscene()
{
	m_CutsceneTrack.Trigger();


}

void audCutsceneAudioEntity::TriggerSyncScene(u32 hash)
{
	caDisplayf("Triggering synched scene with audio hash %u", hash);
	m_SynchedSceneTrack.Trigger(hash);
}

u32 audCutsceneAudioEntity::GetSyncSceneDuration(u32 audioHash)
{
	return m_SynchedSceneTrack.GetSceneAudioDuration(audioHash);
}

u32 audCutsceneAudioEntity::TriggerSyncScene()
{
	caDisplayf("TriggeringSynchedScene with no audio hash");
	u32 sceneHash = 0;
	sceneHash = m_SynchedSceneTrack.GetNextSynchedSceneInQueue();
	if(sceneHash)
	{
		m_SynchedSceneTrack.Trigger(sceneHash);
		return sceneHash;
	}	
	
	return 0;
}

void audCutsceneAudioEntity::HandleSeamlessPlay(s32 slotIndex)
{
#if __BANK
	caDisplayf("Cutscene Handle seamless Play: %s, type %d, slot %d, sound %p, time %d, FC: %u", m_SoundNames[slotIndex], m_CutsceneAudioType[slotIndex], slotIndex, m_SoundNames[slotIndex], fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif

	// The sound should already be playing, but the voices will be in a waiting-for-sync state
	caAssertf(m_Sounds[slotIndex]->GetPlayState() == AUD_SOUND_PLAYING, "Playstate = %d", m_Sounds[slotIndex]->GetPlayState());

	audMetadataObjectInfo info;

#if __BANK
	caDisplayf("Trying to start cutscene dynamic mix scene %s", m_SceneNames[slotIndex]);
#endif

	const u32 sceneHash = atStringHash("_SCENE", m_ScenePartialHashes[slotIndex]);
	if(DYNAMICMIXMGR.GetMetadataManager().GetObjectInfo(sceneHash, info))
	{
		DYNAMICMIXER.StartScene(sceneHash, &m_Scenes[slotIndex]);
	} 
	else if(m_CutsceneAudioType[slotIndex] != AUD_CUTSCENE_TYPE_CUTSCENE)
	{
		DYNAMICMIXER.StartScene(ATSTRINGHASH("SYNCHED_SCENE_DEFAULT_SCENE", 0xE69FFD02), &m_Scenes[slotIndex]);
	}
	else
	{
		DYNAMICMIXER.StartScene(ATSTRINGHASH("CUTSCENE_DEFAULT_SCENE", 0x3E96C108), &m_Scenes[slotIndex]);
	}

	m_NeedToPreloadRadio = true;
	m_PlayedThisFrame = true;

	m_IsPrepared[slotIndex] = false;

	//We are now playing a stream from this wave slot, so toggle the wave slot to be used to preload the next cutscene.
	// - This enables overlapping loads with playback for split cutscenes.
	m_NextWaveSlotIndex = (slotIndex + 1) % 2; 
}

void audCutsceneAudioEntity::StopCutscene(bool isSkipping, u32 releaseTime)
{
	if(isSkipping)
	{
		if(m_Scenes[GetNextWaveslotIndexForPrepare()] && AUD_GET_TRISTATE_VALUE(m_Scenes[GetNextWaveslotIndexForPrepare()]->GetSceneSettings()->Flags, FLAG_ID_MIXERSCENE_CUSCENEAUDIOOVERRUNS) == AUD_TRISTATE_TRUE)
		{
			m_Scenes[GetNextWaveslotIndexForPrepare()]->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), 0.f);
		}
	}

	m_CutsceneTrack.Stop(isSkipping, releaseTime);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CCutSceneStopAudioPacket>(
			CCutSceneStopAudioPacket(isSkipping, releaseTime));
	}
#endif // GTA_REPLAY
}

void audCutsceneAudioEntity::StopOverunAudio()
{
	m_CutsceneTrack.SetCanOverrun(false);
	StopCutscene(false, 10000);
}

void audCutsceneAudioEntity::StopSyncScene(u32 audioHash)
{
	m_SynchedSceneTrack.Stop(audioHash);
}

void audCutsceneAudioEntity::StopSyncScene()
{
	m_SynchedSceneTrack.Stop();
}

u32 audCutsceneAudioEntity::GetCutsceneTime()
{
	return m_CutsceneTrack.GetTrackTime();
}

u32 audCutsceneAudioEntity::GetSyncSceneTime(u32 audioHash)
{
	return m_SynchedSceneTrack.GetSceneTime(audioHash);
}

bool audCutsceneAudioEntity::SlotIsPlaying(u32 slotIndex)
{
	if(m_Sounds[slotIndex])
	{
		return m_Sounds[slotIndex]->GetPlayState() == AUD_SOUND_PLAYING;
	}

	return false;
}


////// audCutTrack /////////////////////////////////////////////////////////////////////////

void audCutTrack::Reset()
{
#if __BANK
	caDisplayf("Cutscene track reset, time %d, FC: %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif

	for(int i=0; i<kNumAudCutsceneEvents; i++)
	{
		m_EventList[i].Init();
	}

	m_IsControllingCutsceneAudio = false;
}

void audCutTrackCutScene::Reset()
{
	audCutTrack::Reset();
	m_State = UNINITIALIZED;
	m_LoadEventIndex = 0;
	m_PlayEventIndex = -1;
	m_NumEvents = 0;
	m_CutsceneLoadIndex = -1;
	m_CutscenePlayIndex = -1;
	m_ScriptSuffix[0] = '\0';
	m_CanOverrun = false;
#if GTA_REPLAY
	m_StartCutsceneTime = 0;
#endif
	
	//We rely on these to keep track of audio time if audio stops so don't reset.
	//m_TriggerTime = 0;
	//m_LastEngineTime = 0;
	//m_LastSoundTime = 0;
}

bool audCutTrackCutScene::IsPrepared()
{
	if(m_State == UNINITIALIZED)
	{
#if __BANK
		caDebugf2("Cutscene audio was asked if it was prepared and it replied: false, time %d, FC: %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif
		return false;
	}

#if __BANK
	if(m_State >= PREPARED)
	{
		caDisplayf("Cutscene audio was asked if it was prepared and it replied: true, time %d, FC: %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
	}
	else
	{
		caDebugf2("Cutscene audio was asked if it was prepared and it replied: false, time %d, FC: %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
	}
#endif

	return m_State >= PREPARED;
}

void audCutTrackCutScene::Init(u32 skipTimeInMs)
{
	if(m_State != audCutTrack::UNINITIALIZED)
	{
		return;
	}

#if !__FINAL
	m_Scrubbingoffset = skipTimeInMs;
#endif

	CutSceneManager * manager = CutSceneManager::GetInstance();
	const cutfCutsceneFile2* cutfile = static_cast<const CutSceneManager*>(CutSceneManager::GetInstance())->GetCutfFile();
	//const cutfCutsceneFile2* cutfile = manager->GetCutfFile(); 

	atArray<audCutSceneEvent> audioEventList;

	const atArray<cutfEvent *> &eventList = cutfile->GetEventList();
	atArray<cutfObject *> objectList;
	cutfile->FindObjectsOfType(CUTSCENE_AUDIO_OBJECT_TYPE, objectList);

	bool isConcatSkip = false; 

	m_LastCutsceneTime = 0;

	atFixedArray<audIntermediateCutsceneEvent, 32> intermediateArray;

	int sectionCount = 0;
	cutfEvent * currentCutEvent = NULL;
	while(sectionCount >= 0)
	{
		audIntermediateCutsceneEvent& thisIntEvent = intermediateArray.Append();
		thisIntEvent.concatSection = sectionCount;
		thisIntEvent.triggerTime = manager->IsConcatted()? manager->GetConcatSectionStartTime(sectionCount) : manager->GetStartTime();
		for(int i=0; i < eventList.GetCount(); i++)
		{
			if(eventList[i]->GetEventId() == CUTSCENE_PLAY_AUDIO_EVENT && (!manager->IsConcatted() || sectionCount == manager->GetConcatSectionForTime(eventList[i]->GetTime())))
			{
				currentCutEvent = eventList[i];
				break;
			}
		}

		thisIntEvent.cutEvent = currentCutEvent;
		sectionCount = manager->IsConcatted() ? manager->GetNextConcatSection(sectionCount) : -1;
	}

#if __BANK
	for(int i=0; i<intermediateArray.GetCount(); i++)
	{
		if(intermediateArray[i].cutEvent)
		{
			caDisplayf("Intermediate event: section %d, isValid=%s, event time %f, cut time %f", intermediateArray[i].concatSection, manager->IsConcatSectionValidForPlayBack(intermediateArray[i].concatSection)? "true":"false", intermediateArray[i].cutEvent ? intermediateArray[i].cutEvent->GetTime() : -1.f, intermediateArray[i].triggerTime); 
		}
	}
#endif

	u32 timeToskip = 0;
	for(int i=0; i<intermediateArray.GetCount(); i++)
	{
		BANK_ONLY(int eventCount = 0;)
		cutfEvent * event = intermediateArray[i].cutEvent;
		if(event && event->GetEventId() == CUTSCENE_PLAY_AUDIO_EVENT)
		{
			BANK_ONLY(eventCount++;)
            const cutfObjectIdEvent* pObjectIdEvent = static_cast<const cutfObjectIdEvent*>( event );

			for(int j=0; j<objectList.GetCount(); j++)
			{
				cutfObject * object = objectList[j];
				if(object->GetObjectId() == pObjectIdEvent->GetObjectId())
				{
					audCutSceneEvent audioEvent;

					int concat = 0;
					u32 duration = 0;
					if(manager->IsConcatted())
					{
						concat = intermediateArray[i].concatSection;
						duration = (u32)(manager->GetConcatSectionDuration(concat)*1000);
						if(!manager->IsConcatSectionValidForPlayBack(concat))
						{
#if __BANK
							caDisplayf("Concat section %d at time %f player event %d is invalid - skipping", concat, event->GetTime(), eventCount);
#endif
							if(!isConcatSkip)
							{
								isConcatSkip = true;
								timeToskip = i;
							}
							break;
						}
					}
					else
					{
						duration = (u32)(manager->GetTotalSeconds()*1000);
					}
					const cutfAudioObject* audioObj = static_cast<const cutfAudioObject*>(object); 

					char soundName[64];
					formatf(soundName, "%s%s", g_CutsceneSoundNamePrefix, object->GetDisplayName().c_str());
					strupr(soundName);

					char * validationStr = NULL;

					validationStr = strstr(soundName, "_EDITED");
					if(validationStr)
					{
						caAssertf(0, "Cutscene audio (%s) should not be named _EDITED, please bug Default Animation Resource", soundName);
						validationStr[0] = '\0';
					}
					validationStr = strstr(soundName, "_MASTERED");
					if(validationStr)
					{
						caAssertf(0, "Cutscene audio (%s) should not be named _MASTERED, please bug Default Animation Resource", soundName);
						validationStr[0] = '\0';
					}

					char sceneName[64];
					char nonSeqName[64];

					//Remove _SEQ for scenes if present
					formatf(nonSeqName, "%s", object->GetDisplayName().c_str());
					char * seqStr = strstr(nonSeqName, "_SEQ");
					if(seqStr)
					{
						seqStr[0] = '\0'; 
					}
					formatf(sceneName, "%s", nonSeqName); 

					audioEvent.PartialHash = atPartialStringHash(soundName);
					audioEvent.ScenePartialHash = atPartialStringHash(sceneName);

					f32 audioOffsetTime = audioObj->GetOffset();

					if((/*sysBootManager::IsBootedFromDisc() || RSG_FINAL ||*/ PARAM_forcetrimmedcutscenes.Get()))
					{
						audioOffsetTime = 0.f;
					}
					else
					{
						const u32 sceneHash = atStringHash("_SCENE", atPartialStringHash(sceneName));
						MixerScene * mixScene = DYNAMICMIXMGR.GetObject<MixerScene>(sceneHash);
						if(mixScene && AUD_GET_TRISTATE_VALUE(mixScene->Flags, FLAG_ID_MIXERSCENE_CUTSCENETRIMMEDASSET) == AUD_TRISTATE_TRUE)
						{
#if __BANK
							caDisplayf("Overriding start offset from MixerScene for trimmed asset: original offset %f", audioOffsetTime);
#endif
							audioOffsetTime = 0.f;
						}

					}

					audioEvent.AudioOffset = (u32)((audioOffsetTime)*1000);			
					audioEvent.StartOffset = (u32)((audioOffsetTime+intermediateArray[i].triggerTime)*1000) + 1; //+1ms to circumvent fp rounding error when we do a concat jump.
					if(isConcatSkip)
					{
						audioEvent.TriggerTime = (u32)((intermediateArray[timeToskip].triggerTime)*1000); //timeToskip;
					}
					else
					{
						audioEvent.TriggerTime = (u32)(intermediateArray[i].triggerTime*1000);
					}
					timeToskip = 0;
					audioEvent.IsConcatSkip = isConcatSkip;
					isConcatSkip = false;
#if __BANK
					caDisplayf("Cutfile event: trigger time %u, object offset %f, event time %f, event %d, concat %d", audioEvent.TriggerTime, audioObj->GetOffset(), event->GetTime(), eventCount, concat);
#endif
#if 1//__BANK
					formatf(audioEvent.TrackName, object->GetDisplayName().c_str());
					formatf(audioEvent.SceneName, sceneName);
#endif
					audioEventList.Grow() = audioEvent;
				}
			}
		}
	}

	audCutSceneEvent * prevEvent = NULL;
	for(int i=0; i<audioEventList.GetCount(); i++)
	{
		audCutSceneEvent * event = &audioEventList[i];
		if(prevEvent)
		{
			//caAssertf((prevEvent->TriggerTime + prevEvent->Duration) == (event->TriggerTime -1), "Non concat-skip discontinuity when generating cutscene timeline for %s, prev trigger %d, prev duration %d, trigger %d", manager->GetCutsceneName(), prevEvent->TriggerTime, prevEvent->Duration, event->TriggerTime);
			if( event->TriggerTime == (u32)(-1000) || (!event->IsConcatSkip && event->PartialHash == prevEvent->PartialHash))
			{
				event->PartialHash = 0;
				continue;
			}
		}
		prevEvent = event;
	}

	u32 totalDuration = (u32)(manager->GetTotalSeconds()*1000.f);
	for(int i = audioEventList.GetCount()-1; i>=0; i--)
	{
		if(audioEventList[i].PartialHash)
		{
			audioEventList[i].Duration = totalDuration - m_EventList[i].GetCutsceneTimeOffset();
			totalDuration -= m_EventList[i].Duration;
		}
	}

	for(int i=0; i<audioEventList.GetCount(); i++)
	{
		if(audioEventList[i].PartialHash)
		{
			if(skipTimeInMs > audioEventList[i].GetCutsceneTimeOffset())
			{
				if(skipTimeInMs > audioEventList[i].GetCutsceneTimeOffset() + audioEventList[i].Duration)
				{
					continue;
				}
				else
				{
					audioEventList[i].StartOffset += (skipTimeInMs - audioEventList[i].GetCutsceneTimeOffset());
					audioEventList[i].TriggerTime += (skipTimeInMs - audioEventList[i].GetCutsceneTimeOffset());
					skipTimeInMs = 0;
				}
			}
			else
			{
				skipTimeInMs = 0;
			}
#if __BANK
			caDisplayf("Event %s, Start offset %d, trigger time %d, Duration %d is concat skip %s", audioEventList[i].TrackName, audioEventList[i].StartOffset, audioEventList[i].TriggerTime, audioEventList[i].Duration, audioEventList[i].IsConcatSkip?"true":"false");
#endif

#if 0 // __ASSERT
			u32 soundDuration = g_CutsceneAudioEntity.GetDuration(audioEventList[i].SceneName) - audioEventList[i].AudioOffset;
			u32 cutsceneDuration = (u32)(manager->GetTotalSeconds()*1000.f);
			caAssertf(soundDuration < cutsceneDuration, "Mismatch in cutscene/audio asset duration for audio %s : %d, cutscene %s : %d, please check that the assets match up (puls any EDITED or MASTERED versions)", audioEventList[i].SceneName, soundDuration, manager->GetCutsceneName(), cutsceneDuration);
#endif
			if(audioEventList[i].TriggerTime == 0)
			{
				audioEventList[i].IsConcatSkip = false;
			}
			m_EventList[m_NumEvents++] = audioEventList[i];
		}
	}

	if(m_NumEvents)
	{
		m_LastSoundTime = m_EventList[0].GetCutsceneTimeOffset();
	}

	m_State = INITIALIZED;
}

void audCutTrackCutScene::Update()
{
	switch(m_State)
	{
	case audCutTrack::UNINITIALIZED:
		break;
	case audCutTrack::INITIALIZED:
		{
			if(g_CutsceneAudioEntity.CutsceneAudioIsAvailable())
			{
				//TODO check synched scenes aren't active when support is added for them
				if(g_CutsceneAudioEntity.SlotIsFree(g_CutsceneAudioEntity.GetWaveslotIndexForPrepare()))
				{
					m_CutsceneLoadIndex = g_CutsceneAudioEntity.GetWaveslotIndexForPrepare();
				}
				else if(g_CutsceneAudioEntity.SlotIsFree(g_CutsceneAudioEntity.GetNextWaveslotIndexForPrepare()))
				{
					m_CutsceneLoadIndex = g_CutsceneAudioEntity.GetNextWaveslotIndexForPrepare();
				}
		
				if(m_CutsceneLoadIndex >= 0)
				{
#if __BANK
					caDisplayf("Begin prepare with load index %d, time %d, FC: %u", m_CutsceneLoadIndex, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif 

					m_State = audCutTrack::PREPARING;
					SetIsControllingCutsceneAudio(true);
				}
			}
		}
		break;
	case audCutTrack::PREPARING:
		{
			if(g_CutsceneAudioEntity.Prepare(m_EventList[m_LoadEventIndex], AUD_CUTSCENE_TYPE_CUTSCENE, m_CutsceneLoadIndex)!= AUD_PREPARING)
			{
#if __BANK
				caDisplayf("Prepared and activating cutscene track at load index %d, time %d, FC: %u", m_LoadEventIndex, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif
				m_State = audCutTrack::PREPARED;
			}		
		}
		break;
	case audCutTrack::PREPARED:
		break;
	case audCutTrack::ACTIVE:
		{
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->GetSceneHashString()->IsNotNull())
			{
				CReplayMgr::RecordFx<CCutSceneAudioPacket>(
					CCutSceneAudioPacket(GetTrackTime(), m_StartCutsceneTime));
			}
#endif // GTA_REPLAY


			if(m_CutsceneLoadIndex != -1)
			{
				if(!g_CutsceneAudioEntity.IsPrepared(m_CutsceneLoadIndex))
				{
					g_CutsceneAudioEntity.Prepare(m_EventList[m_LoadEventIndex], AUD_CUTSCENE_TYPE_CUTSCENE, m_CutsceneLoadIndex);
				}

	/*			if(!g_CutsceneAudioEntity.SlotIsFree(m_CutsceneLoadIndex) && g_CutsceneAudioEntity.IsPrepared(m_CutsceneLoadIndex) && g_CutsceneAudioEntity.GetPlayTimeMs(m_CutsceneLoadIndex) > 0)
				{
					g_CutsceneAudioEntity.HandleSeamlessPlay(m_CutsceneLoadIndex);
					if(!g_CutsceneAudioEntity.SlotIsFree(m_CutscenePlayIndex))
					{
						g_CutsceneAudioEntity.Stop(false, m_CutscenePlayIndex);
					}
					HandlePlay();
				}*/
				/*else if(m_CutscenePlayIndex < 0 || g_CutsceneAudioEntity.SlotIsFree(m_CutscenePlayIndex)) //If other sound stops or trigger is missed lets just play the next sound
				{
					if(g_CutsceneAudioEntity.IsPrepared(m_CutsceneLoadIndex) && )
					{
						g_CutsceneAudioEntity.Play(m_CutsceneLoadIndex);
						HandlePlay();
					}
				}*/
				else if (GetTrackTime(true) >= m_EventList[m_LoadEventIndex].TriggerTime)
				{
					if (g_GameTimerCutscenes || PARAM_gametimercutscenes.Get())
					{
						HandlePlay();
					}
					else if(g_CutsceneAudioEntity.IsPrepared(m_CutsceneLoadIndex))
					{
						g_CutsceneAudioEntity.Stop(false, m_CutscenePlayIndex, 10);
						
						if(g_CutsceneAudioEntity.SlotIsPlaying(m_CutsceneLoadIndex))
						{
							g_CutsceneAudioEntity.Play(m_CutsceneLoadIndex);
						}
						HandlePlay();
					}					
					else
					{
						caErrorf("Want to do a concat jump but audio isn't prepared yet");
					}
				}
			}
		}
		break;
	case audCutTrack::ENDING:
		{
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->GetSceneHashString()->IsNotNull())
			{
				CReplayMgr::RecordFx<CCutSceneAudioPacket>(
					CCutSceneAudioPacket(GetTrackTime(), m_StartCutsceneTime));
			}
#endif // GTA_REPLAY

			SetIsControllingCutsceneAudio(false);
			if(m_CutscenePlayIndex <0 || g_CutsceneAudioEntity.SlotIsFree(m_CutscenePlayIndex))
			{
				Reset();
			}
		}
	default:
		break;
	}
}

void audCutTrackCutScene::Stop()
{
	if(m_CutsceneLoadIndex >= 0)
	{
		g_CutsceneAudioEntity.Stop(false, m_CutsceneLoadIndex);
	}
	if(m_CutscenePlayIndex >= 0)
	{
		g_CutsceneAudioEntity.Stop(false, m_CutscenePlayIndex);
	}

	Reset();
}

void audCutTrackCutScene::Stop(bool isSkipping, u32 release)
{
#if __BANK
	caDisplayf("Cutscene track stop called at time %d, FC: %u, can overrun = %s, is skipping = %s", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount(), FMT_BOOL(m_CanOverrun), FMT_BOOL(isSkipping));
#endif

	if(g_CutsceneAudioEntity.GetMixScene(g_CutsceneAudioEntity.GetNextWaveslotIndexForPrepare()) && AUD_GET_TRISTATE_VALUE(g_CutsceneAudioEntity.GetMixScene(g_CutsceneAudioEntity.GetNextWaveslotIndexForPrepare())->GetSceneSettings()->Flags, FLAG_ID_MIXERSCENE_CUTSCENEQUICKRELEASE) == AUD_TRISTATE_TRUE)
	{
		release = 10;
	}

	if(!m_CanOverrun || isSkipping)
	{
		if(m_CutsceneLoadIndex >= 0)
		{
			g_CutsceneAudioEntity.Stop(false, m_CutsceneLoadIndex, release);
		}
		if(m_CutscenePlayIndex >= 0)
		{
			g_CutsceneAudioEntity.Stop(false, m_CutscenePlayIndex, release);
		}

		Reset();
	}
	else // m_CanOverrun && !isSkipping 
	{
		if(m_CutsceneLoadIndex >= 0)
		{
			if(g_CutsceneAudioEntity.GetMixScene(m_CutsceneLoadIndex))
			{
				g_CutsceneAudioEntity.GetMixScene(m_CutsceneLoadIndex)->Stop();
			}
		}

		if(m_CutscenePlayIndex >= 0)
		{
			if(g_CutsceneAudioEntity.GetMixScene(m_CutscenePlayIndex))
			{
				g_CutsceneAudioEntity.GetMixScene(m_CutscenePlayIndex)->Stop();
			}
		}
	}
}

void audCutTrackCutScene::HandlePlay()
{
	if(g_CutsceneAudioEntity.SlotIsPlaying(m_CutsceneLoadIndex))
	{
		m_CutscenePlayIndex = m_CutsceneLoadIndex;
	}
	else
	{
		g_CutsceneAudioEntity.Stop(false, m_CutsceneLoadIndex);
		m_CutscenePlayIndex = -1;
	}
	m_PlayEventIndex = m_LoadEventIndex;
	m_LoadEventIndex++;

	m_LastSoundTime = m_EventList[m_PlayEventIndex].GetCutsceneTimeOffset();
	m_LastEngineTime = g_CutsceneAudioEntity.GetEngineTime();
	
	if(m_EventList[m_PlayEventIndex].IsConcatSkip)
	{
		g_DidCutsceneJumpThisFrame = true;
	}

	if(m_LoadEventIndex < m_NumEvents)
	{
		m_CutsceneLoadIndex = (m_CutsceneLoadIndex+1)&1;
	}
	else
	{
		m_CutsceneLoadIndex = -1;
		m_State = audCutTrack::ENDING;
	}
}

void audCutTrackCutScene::Trigger()
{
	if(m_State == audCutTrack::PREPARED)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer)
		{
			pPlayer->GetFootstepHelper().ResetFlags();
		}
		g_CutsceneAudioEntity.Play(m_CutsceneLoadIndex);
		m_TriggerTime = g_CutsceneAudioEntity.GetEngineTime();
		m_LastEngineTime = m_TriggerTime;
		m_State = audCutTrack::ACTIVE;
		HandlePlay();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->GetSceneHashString()->IsNotNull())
		{
			m_StartCutsceneTime = GetTrackTime();
			CReplayMgr::RecordFx<CCutSceneAudioPacket>(
				CCutSceneAudioPacket(GetTrackTime(), m_StartCutsceneTime));
		}
#endif // GTA_REPLAY
	}
}

u32 audCutTrackCutScene::GetTrackTime(bool isUpdate)
{
	if(m_NumEvents > 0)
	{
		if(m_State <= PREPARED && m_State > UNINITIALIZED)
		{
			m_LastCutsceneTime = m_EventList[0].GetCutsceneTimeOffset();
			return m_LastCutsceneTime;
		}


		u32 calcSoundTime = 0, catchupTime = 0;
		u32 engineTime = g_CutsceneAudioEntity.GetEngineTime() - m_LastEngineTime + m_LastSoundTime;
		u32 timeToUse = engineTime;

		if(m_CutscenePlayIndex > -1)
		{
			s32 soundTime = g_CutsceneAudioEntity.GetPlayTimeIncludingOffsetMs(m_CutscenePlayIndex);
			if((g_CutsceneAudioEntity.GetPlayTimeMs(m_CutscenePlayIndex)) > 0)
			{
				calcSoundTime =  soundTime - m_EventList[m_PlayEventIndex].AudioOffset;
				timeToUse = calcSoundTime;
				m_LastSoundTime = calcSoundTime;
				m_LastEngineTime = g_CutsceneAudioEntity.GetEngineTime();

				if(!isUpdate && m_State == ACTIVE && calcSoundTime >= m_EventList[m_LoadEventIndex].TriggerTime)
				{
					if(g_CutsceneAudioEntity.IsPrepared(m_CutsceneLoadIndex))
					{
						g_CutsceneAudioEntity.Stop(false, m_CutscenePlayIndex, 10);

						if(g_CutsceneAudioEntity.SlotIsPlaying(m_CutsceneLoadIndex))
						{
							g_CutsceneAudioEntity.Play(m_CutsceneLoadIndex);
						}
						HandlePlay();

						timeToUse = m_LastSoundTime;

					}
					else
					{
						caErrorf("Want to do a concat jump but audio isn't prepared yet");
					}
				}
			}
			else 
			{
				catchupTime = (u32)((g_CutsceneAudioEntity.GetEngineTime() - m_LastEngineTime )*g_CutsceneAudioCatchupFactor) + m_LastSoundTime; 
				timeToUse = catchupTime;
			}
		}

#if __BANK
		caDebugf3("Cutscene time %d, m_State: %d, calcSoundTime: %d, catchupTime %d, engineTime %d, game time %d", timeToUse, m_State, calcSoundTime, catchupTime, engineTime, fwTimer::GetTimeInMilliseconds()); 
#endif
		//Last line of defence against time going backwards...
		if(timeToUse < m_LastCutsceneTime)
		{
			caErrorf("Cutscene time has gone backwards; last time (%u), current time (%u), FC:%u", m_LastSoundTime, timeToUse, fwTimer::GetFrameCount());
			timeToUse = m_LastCutsceneTime + (u32)((g_CutsceneAudioEntity.GetEngineTime() - m_LastEngineTime )*g_CutsceneAudioCatchupFactor);
		}

		m_LastCutsceneTime = timeToUse;

		return timeToUse; 
	}
	else
	{
		if(m_State <= PREPARED && m_State > UNINITIALIZED)
		{
			return 0 
#if !__FINAL
				+ m_Scrubbingoffset
#endif
				;
		}

		s32 offset = 0;

#if !__FINAL
		if(m_Scrubbingoffset)
		{
			offset = m_Scrubbingoffset - m_TriggerTime;
		}
		else
#endif
		{
			offset = m_LastSoundTime - m_LastEngineTime;
		}

#if __BANK
		caDebugf3("Fake cutscene time %d, m_State: %d", g_CutsceneAudioEntity.GetEngineTime() + offset, m_State); 
#endif

		u32 timeToUse = g_CutsceneAudioEntity.GetEngineTime() + offset;

		//Last line of defence against time going backwards...
		if(timeToUse < m_LastCutsceneTime)
		{
			caErrorf("Cutscene time has gone backwards; last time (%u), current time (%u)", m_LastSoundTime, timeToUse);
			timeToUse = m_LastCutsceneTime + (u32)((g_CutsceneAudioEntity.GetEngineTime() - m_LastEngineTime )*g_CutsceneAudioCatchupFactor);
		}

		m_LastCutsceneTime = timeToUse;

		return timeToUse;
	}

}

void audCutTrackCutScene::SkipToTime(u32 timInMs)
{
	Stop();
	Init(timInMs);
}



bool audCutTrackSynchedScene::IsPrepared()
{
	caErrorf("Call audCutTrackSynchedScene::IsPrepared(u32 hash) instead");
	return false;
}

void audCutTrackSynchedScene::Trigger()
{
	u32 triggerTime = ~0U;
	int indexToTrigger = -1;
	for(int i=0; i<kNumAudSynchedSceneEvents; i++)
	{
		if(m_EventStates[i] == PREPARED && m_EventList[i].TriggerTime < triggerTime)
		{
			triggerTime = m_EventList[i].TriggerTime;
			indexToTrigger = i;
		}
	}

	if(indexToTrigger >= 0)
	{
		Trigger(indexToTrigger);
	}
}

void audCutTrackSynchedScene::Stop()
{
	for(int index = 0; index < kNumAudSynchedSceneEvents; index++)
	{
		if(m_CutsceneIndexes[index] >= 0)
		{
			g_CutsceneAudioEntity.Stop(false, m_CutsceneIndexes[index]);
		}
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CSynchSceneStopAudioPacket>(
				CSynchSceneStopAudioPacket(0));
		}
#endif // GTA_REPLAY
	}

	//Reset all the synch scene, this makes sure SetIsControllingCutsceneAudio is cleaned up.
	Reset();
}

void audCutTrackSynchedScene::Stop(u32 hash)
{
	int index = GetEventIndexFromHash(hash);
	if(index >= 0)
	{
		if(m_CutsceneIndexes[index] >= 0)
		{
			g_CutsceneAudioEntity.Stop(false, m_CutsceneIndexes[index]);
		}
		Reset(index);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CSynchSceneStopAudioPacket>(
				CSynchSceneStopAudioPacket(hash));
		}
#endif // GTA_REPLAY
	}
}

bool audCutTrackSynchedScene::IsPrepared(const char *audioName)
{
	int index = GetEventIndexFromHash(atStringHash(audioName));
	if(index >= 0 && m_CutsceneIndexes[index] >=0)
	{
		return m_EventStates[index] >= audCutTrack::PREPARED;
	}
	else
	{
		Init(audioName);
		return false;
	}
}

void audCutTrackSynchedScene::Trigger(u32 nameHash)
{
	int index = GetEventIndexFromHash(nameHash);
	if(index >= 0)
	{
		if(m_EventStates[index] == PREPARED)
		{
			g_CutsceneAudioEntity.Play(m_CutsceneIndexes[index]);
			m_TriggerTimes[index] = g_CutsceneAudioEntity.GetEngineTime();
			m_EventStates[index] = audCutTrack::ENDING; //Synched scenes only have one event but multiple scenes can be prepared and triggered
			
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CSynchSceneAudioPacket>(
					CSynchSceneAudioPacket(nameHash, m_EventList[index].TrackName, GetSceneTime(atFinalizeHash(m_EventList[index].ScenePartialHash)), m_UseScenePosistion[index], m_UseSceneEntity[index], VEC3V_TO_VECTOR3(GetPlayPosition(index))));
			}
#endif // GTA_REPLAY

			if(!g_CutsceneAudioEntity.SlotIsPlaying(m_CutsceneIndexes[index]))
			{
				g_CutsceneAudioEntity.Stop(false, m_CutsceneIndexes[index]);
				m_CutsceneIndexes[index] = -1;
			}
		}
	}
}

int audCutTrackSynchedScene::GetEventIndexFromHash(u32 nameHash)
{
	for(int i=0; i< kNumAudSynchedSceneEvents; i++)
	{
		if(atFinalizeHash(m_EventList[i].ScenePartialHash) == nameHash)
		{
			return i;
		}
	}

	return -1;
}

bool audCutTrackSynchedScene::HasSyncSceneBeenInitialized(const char* sceneName)
{
	int sceneIndex = GetEventIndexFromHash(atStringHash(sceneName));

	if(sceneIndex < 0)
	{
		return false;
	}

	if(m_EventStates[sceneIndex] >= INITIALIZED && m_EventStates[sceneIndex] <= ENDING )
	{
		return true;
	}

	return false;
}

bool audCutTrackSynchedScene::HasSyncSceneBeenTriggered(const char* sceneName)
{
	int sceneIndex = GetEventIndexFromHash(atStringHash(sceneName));

	if(sceneIndex < 0)
	{
		return false;
	}

	if(m_EventStates[sceneIndex] == ENDING )
	{
		return true;
	}

	return false;
}

void audCutTrackSynchedScene::Init(const char* sceneName, u32 skipMs, u32 parentHash)
{
	int sceneIndex = -1;
	
	if(GetEventIndexFromHash(atStringHash(sceneName)) >= 0)
	{
		return;
	}

	for(int i=0; i<kNumAudSynchedSceneEvents; i++)
	{
		if(m_EventStates[i] == UNINITIALIZED)
		{
			sceneIndex = i;
			break;
		}
	}

	if(sceneIndex<0)
	{
		caErrorf("Trying to init synched scene audio %s but there's no free slots", sceneName);
		return;
	}

	if(parentHash)
	{
		int parentIndex = GetEventIndexFromHash(parentHash);
		if(parentIndex >= 0)
		{
			m_SynchSceneToTrigger[parentIndex] = sceneIndex;
		}
	}

	m_EventList[sceneIndex].StartOffset = skipMs;

	char soundName[64];
	formatf(soundName, "%s%s", g_CutsceneSoundNamePrefix, sceneName);
	strupr(soundName);

	char * validationStr = NULL;

	validationStr = strstr(soundName, "_CUSTOM");
	if(validationStr)
	{
		caAssertf(0, "Cutscene audio (%s) should not be named _CUSTOM, please bug Default Animation Resource", soundName);
		validationStr[0] = '\0';
	}

	char mixSceneName[64];
	char nonSeqName[64];


	//Remove _SEQ for scenes if present
	formatf(nonSeqName, "%s", sceneName);
	char * seqStr = strstr(nonSeqName, "_SEQ");
	if(seqStr)
	{
		seqStr[0] = '\0'; 
	}
	formatf(mixSceneName, "%s", nonSeqName); 


	m_EventList[sceneIndex].PartialHash = atPartialStringHash(soundName);
	m_EventList[sceneIndex].ScenePartialHash = atPartialStringHash(mixSceneName);
	m_EventList[sceneIndex].TriggerTime = g_CutsceneAudioEntity.GetEngineTime();


#if __BANK
	caDisplayf("Synched scene event %s: init time %u, FC: %u", sceneName, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif
#if 1//__BANK
	formatf(m_EventList[sceneIndex].TrackName, sceneName);
	formatf(m_EventList[sceneIndex].SceneName, mixSceneName);
#endif

	m_EventStates[sceneIndex] = INITIALIZED;

}

void audCutTrackSynchedScene::Update()
{
	for(int i=0; i<kNumAudSynchedSceneEvents; i++)
	{
		switch(m_EventStates[i])
		{
			case audCutTrack::UNINITIALIZED:
		break;
	case audCutTrack::INITIALIZED:
		{
			if(g_CutsceneAudioEntity.CutsceneAudioIsAvailable())
			{
				//TODO check synched scenes aren't active when support is added for them
				if(g_CutsceneAudioEntity.SlotIsFree(g_CutsceneAudioEntity.GetWaveslotIndexForPrepare()))
				{
					m_CutsceneIndexes[i] = g_CutsceneAudioEntity.GetWaveslotIndexForPrepare();
				}
				else if(g_CutsceneAudioEntity.SlotIsFree(g_CutsceneAudioEntity.GetNextWaveslotIndexForPrepare()))
				{
					m_CutsceneIndexes[i] = g_CutsceneAudioEntity.GetNextWaveslotIndexForPrepare();
				}
		
				if(m_CutsceneIndexes[i] >= 0)
				{
#if __BANK
					caDisplayf("Begin SS prepare with load index %d, time %d, FC:%u", m_CutsceneIndexes[i], fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif 

					m_EventStates[i] = audCutTrack::PREPARING;
					SetIsControllingCutsceneAudio(true);
				}
			}
		}
		break;
	case audCutTrack::PREPARING:
		{
			if(g_CutsceneAudioEntity.Prepare(m_EventList[i], AUD_CUTSCENE_TYPE_SYNCHED_SCENE, m_CutsceneIndexes[i]) != AUD_PREPARING)
			{
#if __BANK
				caDisplayf("Prepared and activating synched scene track at load index %d, sound %s (%p), time %d, FC: %u", i, g_CutsceneAudioEntity.GetSoundName(m_CutsceneIndexes[i]), g_CutsceneAudioEntity.GetSound(m_CutsceneIndexes[i]), fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
#endif
				m_EventStates[i] = audCutTrack::PREPARED;
				SetIsControllingCutsceneAudio(false);
			}		
		}
		break;
	case audCutTrack::PREPARED:
		break;
	case audCutTrack::ACTIVE:
		{
			caErrorf("Synched scene audio %s is ACTIVE but should be ENDING", m_EventList[i].TrackName);
		}
		break;
	case audCutTrack::ENDING:
		{
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CSynchSceneAudioPacket>(
					CSynchSceneAudioPacket(atFinalizeHash(m_EventList[i].ScenePartialHash), m_EventList[i].TrackName, GetSceneTime(atFinalizeHash(m_EventList[i].ScenePartialHash)), m_UseScenePosistion[i], m_UseSceneEntity[i], VEC3V_TO_VECTOR3(GetPlayPosition(i))), GetPlayEntity(i));
			}
#endif // GTA_REPLAY

			if(m_CutsceneIndexes[i] <0 || g_CutsceneAudioEntity.SlotIsFree(m_CutsceneIndexes[i]))
			{
				
				if(m_SynchSceneToTrigger[i] > -1)
				{
					g_CutsceneAudioEntity.TriggerSyncScene(atFinalizeHash(m_EventList[m_SynchSceneToTrigger[i]].ScenePartialHash));
				}
				else if(m_TriggerCutscene)
				{
					g_CutsceneAudioEntity.TriggerCutscene();
				}
				m_CutsceneIndexes[i] = -1;
				//Reset(i);
			}
		}
	default:
		break;
		}
	}
}

void audCutTrackSynchedScene::SkipToTime(u32 audioHash, u32 skipMs)
{
	int eventIndex = GetEventIndexFromHash(audioHash);
	if(eventIndex >=0)
	{
		char sceneName[128] = {0};
		formatf(sceneName, "%s", m_EventList[eventIndex].SceneName);
		Stop(audioHash);
		Init(sceneName, skipMs);
	}
}

void audCutTrackSynchedScene::Reset()
{
	audCutTrack::Reset();
	for(int i=0; i<kNumAudSynchedSceneEvents; i++)
	{
		Reset(i);
	}
}

void audCutTrackSynchedScene::Reset(int index)
{
	m_EventList[index].Init();
	m_EventStates[index] = UNINITIALIZED;
	m_CutsceneIndexes[index] = -1;
	m_TriggerTimes[index] = 0;
	m_SynchSceneToTrigger[index] = -1;
	m_TriggerCutscene = false;
	m_LastSoundTimes[index] = 0;
	m_LastEngineTimes[index] = 0;
	m_UseScenePosistion[index] = false;
	m_UseSceneEntity[index] = false;
}

void audCutTrackSynchedScene::ClearUnusedEntities()
{
	for(int i=0; i < kNumAudSynchedSceneEvents; i++)
	{
		if(!m_UseSceneEntity[i] && m_SceneEntity[i])
		{
			m_SceneEntity[i] = NULL;
		}
	}
}

u32 audCutTrackSynchedScene::GetNextSynchedSceneInQueue()
{
	u32 triggerTime = ~0U;
	int index = -1;
	for(int i=0; i<kNumAudSynchedSceneEvents; i++)
	{
		if(m_EventStates[i] == PREPARED && m_EventList[i].TriggerTime < triggerTime)
		{
			index = i;
			triggerTime = m_EventList[i].TriggerTime;
		}
	}

	if(index < 0)
	{
		caErrorf("No prepared synched scene audio available");
		return 0;
	}

	return atFinalizeHash(m_EventList[index].ScenePartialHash);
}

u32 audCutTrackSynchedScene::GetSceneTime(u32 audioHash)
{
	int index = GetEventIndexFromHash(audioHash);
	if(index >= 0)
	{
		if(m_EventStates[index] <= audCutTrack::PREPARED)
		{
			return 0;
		}


		u32 calcSoundTime = 0;
		u32 engineTime = g_CutsceneAudioEntity.GetEngineTime() - m_TriggerTimes[index] - m_LastEngineTimes[index] + m_LastSoundTimes[index];
		u32 timeToUse = engineTime;


		if(m_CutsceneIndexes[index] > -1)
		{
			s32 soundTime = g_CutsceneAudioEntity.GetPlayTimeMs(m_CutsceneIndexes[index]) + m_EventList[index].StartOffset;
			if(soundTime > 0)
			{
				calcSoundTime =  soundTime;
				timeToUse = calcSoundTime;
				m_LastSoundTimes[index] = calcSoundTime;
				m_LastEngineTimes[index] = g_CutsceneAudioEntity.GetEngineTime();
			}
		}

#if __BANK
		caDebugf3("sound time %u, engine time %u, timeToUse %u, frameTime %u", calcSoundTime, engineTime, timeToUse, fwTimer::GetTimeInMilliseconds());
#endif

		return timeToUse; 
	}

#if __BANK
		caDebugf3("engine time %u, ", g_CutsceneAudioEntity.GetEngineTime());
#endif

	return g_CutsceneAudioEntity.GetEngineTime();
}

u32 audCutTrackSynchedScene::GetSceneAudioDuration(u32 hash)
{
	int index = GetEventIndexFromHash(hash);
	if(index >= 0)
	{
		const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(atStringHash("_CUSTOM", m_EventList[index].PartialHash)); 

		if (streamingSound) 
		{ 
			// Compute our actual start offset 
			return streamingSound->Duration; 
		} 

		streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(atFinalizeHash(m_EventList[index].PartialHash));

		if (streamingSound)
		{		
			//	Compute our actual start offset
			return streamingSound->Duration;
		}
	}
	return 0;
}

void audCutTrackSynchedScene::ActivateLooping(u32 hash)
{
	int index = GetEventIndexFromHash(hash);
	if(index > -1)
	{
		
	}
}

void audCutTrackSynchedScene::SetEntity(u32 hash, CEntity * entity)
{
	int index = GetEventIndexFromHash(hash);
	if(index > -1)
	{
		m_SceneEntity[index] = entity;
		m_UseSceneEntity[index] = true;
	}
}

void audCutTrackSynchedScene::SetPosition(u32 hash, Vec3V_In position)
{
	int index = GetEventIndexFromHash(hash);
	if(index > -1)
	{
		m_ScenePosition[index] = position;
		m_UseScenePosistion[index] = true;
	}
}
