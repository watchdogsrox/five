//=====================================================================================================
// name:		FootStepPacket.cpp
//=====================================================================================================

#include "SpeechAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "Control/Replay/ReplayInterfacePed.h"
#include "control/replay/ReplayInternal.h"
#include "control/replay/audio/SoundPacket.h"
#include "audio/pedaudioentity.h"
#include "peds/ped.h" 
#include "audio/scriptaudioentity.h"

//========================================================================================================================
CPacketSpeech::CPacketSpeech(const audDeferredSayStruct* m_DeferredSayStruct, bool checkSlotVancant, const Vector3 positionForNullSpeaker, const s32 startOffset)
	: CPacketEvent(PACKETID_SOUND_SPEECH_SAY, sizeof(CPacketSpeech))
	, CPacketEntityInfoStaticAware()
{
	m_contextHash = m_DeferredSayStruct->contextPHash;
	m_voiceNameHash = m_DeferredSayStruct->voiceNameHash;
	m_preDelay = m_DeferredSayStruct->preDelay;
	m_requestedVolume = m_DeferredSayStruct->requestedVolume;
	m_variationNum = m_DeferredSayStruct->variationNumber;
	m_offsetMs = startOffset;
	m_priority = m_DeferredSayStruct->priority;
	m_requestedAudibility = m_DeferredSayStruct->requestedAudibility;
	m_preloadOnly = m_DeferredSayStruct->preloadOnly;
	m_checkSlotVacant = checkSlotVancant;
	m_positionForNullSpeaker = positionForNullSpeaker;

	m_IsMutedDuringSlowMo = (m_DeferredSayStruct->isMutedDuringSlowMo)
		&& (m_DeferredSayStruct->speechContext ? (AUD_GET_TRISTATE_VALUE(m_DeferredSayStruct->speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISMUTEDDURINGSLOWMO)!=AUD_TRISTATE_FALSE) : true)
		&& (!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowAmbientSpeechInSlowMo));

	m_IsPitchedDuringSlowMo = (m_DeferredSayStruct->isPitchedDuringSlowMo) 
		&& (m_DeferredSayStruct->speechContext ? (AUD_GET_TRISTATE_VALUE(m_DeferredSayStruct->speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISPITCHEDDURINGSLOWMO)!=AUD_TRISTATE_FALSE) : true)
		&& (!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowAmbientSpeechInSlowMo));
}

//////////////////////////////////////////////////////////////////////////
void CPacketSpeech::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying() || m_voiceNameHash == 0 || m_contextHash == 0)
	{
		return;
	}

	CPed* pPed = (CPed*)eventInfo.GetEntity(0);
	Vector3 nullSpeakerPosition = ORIGIN;

	nullSpeakerPosition = m_positionForNullSpeaker;

	if(m_offsetMs > 0 && g_ScriptAudioEntity.HasAlreadyPlayed(m_voiceNameHash, m_contextHash, m_variationNum))
	{
		g_ScriptAudioEntity.UpdateTimeStamp(m_voiceNameHash, m_contextHash, m_variationNum);
		return;
	}

	s32 startOffset = m_offsetMs;
	bool preload = m_preloadOnly;
	if(m_offsetMs < 0)
	{
		preload = true;
		startOffset = 0;
	}

	if(pPed) 
	{
		pPed->GetSpeechAudioEntity()->ReplaySay(m_contextHash, m_voiceNameHash, m_preDelay, m_requestedVolume, m_variationNum, m_priority, startOffset, m_requestedAudibility, preload, (CPed*)eventInfo.GetEntity(1), m_checkSlotVacant, nullSpeakerPosition, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo);
	}
	else
	{
		g_ScriptAudioEntity.GetAmbientSpeechAudioEntity().ReplaySay(m_contextHash, m_voiceNameHash, m_preDelay, m_requestedVolume, m_variationNum, m_priority, startOffset, m_requestedAudibility, preload, (CPed*)eventInfo.GetEntity(1), m_checkSlotVacant, nullSpeakerPosition, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo);
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketSpeechPain::CPacketSpeechPain(u8 painLevel, const audDamageStats& damageStats, f32 distToListener, u8 painVoiceType, u8 painType, u8 prevVariation, u8 prevNumTimes, u32 prevNextTimeToLoad, u8 nextVariation, u8 nextNumTimes, u32 nextNextTimeToLoad, u32 lastPainLevel, u32* lastPainTimes)
	: CPacketEvent(PACKETID_SOUND_SPEECH_PAIN, sizeof(CPacketSpeechPain)) 
	, CPacketEntityInfoStaticAware()
	, m_rawDamage(damageStats.RawDamage)
	, m_preDelay(damageStats.PreDelay)
	, m_damageReason((u8)damageStats.DamageReason)
	, m_fatal(damageStats.Fatal)
	, m_pedWasAlreadyDead(damageStats.PedWasAlreadyDead)
	, m_playGargle(damageStats.PlayGargle)
	, m_isHeadshot(damageStats.IsHeadshot)	
	, m_isSilenced(damageStats.IsSilenced)
	, m_isSniper(damageStats.IsSniper)
	, m_isFrontend(damageStats.IsFrontend)
	, m_isFromAnim(damageStats.IsFromAnim)
	, m_forceDeathShort(damageStats.ForceDeathShort)
	, m_forceDeathMedium(damageStats.ForceDeathMedium)
	, m_forceDeathLong(damageStats.ForceDeathLong)
	, m_painLevel(painLevel)
	, m_distToListener(distToListener)
	, m_painVoiceType(painVoiceType)
	, m_painType(painType)
	, m_prevVariationForPain(prevVariation)
	, m_prevNumTimesPlayed(prevNumTimes)
	, m_prevNextTimeToLoadPain(prevNextTimeToLoad)
	, m_nextVariationForPain(nextVariation)
	, m_nextNumTimesPlayed(nextNumTimes)
	, m_nextNextTimeToLoadPain(nextNextTimeToLoad)
	, m_lastPainLevel(lastPainLevel)
{
	m_lastPainTimes[0] = lastPainTimes[0];
	m_lastPainTimes[1] = lastPainTimes[1];
}

//////////////////////////////////////////////////////////////////////////
void CPacketSpeechPain::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	// Only actually play the pain sound if the replay is playing forwards at normal speed
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	if(	(eventInfo.GetPlaybackFlags().GetState() & REPLAY_STATE_PLAY) && 
		(eventInfo.GetPlaybackFlags().GetState() & REPLAY_CURSOR_NORMAL) && 
		(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD))
	{
		CPed* pPed = (CPed*)eventInfo.GetEntity(0);

		if(pPed) 
		{
			audDamageStats damageStats;
			damageStats.RawDamage			= m_rawDamage;
			damageStats.PreDelay			= m_preDelay;
			damageStats.DamageReason		= (audDamageReason)m_damageReason;
			damageStats.Fatal				= m_fatal;
			damageStats.PedWasAlreadyDead	= m_pedWasAlreadyDead;
			damageStats.PlayGargle			= m_playGargle;
			damageStats.IsHeadshot			= m_isHeadshot;
			damageStats.IsSilenced			= m_isSilenced;
			damageStats.IsSniper			= m_isSniper;
			damageStats.IsFrontend			= m_isFrontend;
			damageStats.IsFromAnim			= m_isFromAnim;
			damageStats.ForceDeathShort		= m_forceDeathShort;
			damageStats.ForceDeathMedium	= m_forceDeathMedium;
			damageStats.ForceDeathLong		= m_forceDeathLong;

			pPed->m_pSpeechAudioEntity->SetLastPainLevel(m_lastPainLevel);
			pPed->m_pSpeechAudioEntity->SetLastPainTimes(m_lastPainTimes[0], m_lastPainTimes[1]);

			bool actuallyPlayedSomething = pPed->m_pSpeechAudioEntity->PlayPain(m_painLevel, damageStats, m_distToListener);

			if(actuallyPlayedSomething)
			{		
				pPed->SetIsWaitingForSpeechToPreload(false);
			}
		}
	}

	bool isJumpingOrRwnd = (eventInfo.GetPlaybackFlags().GetState() & REPLAY_CURSOR_JUMP) || (eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK);
	// Here we call our version of 'PlayedPain' depending on the direction
	// This is regardless of the speed of playback or jumping
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD)
		g_SpeechManager.PlayedPainForReplay(m_painVoiceType, m_painType, m_nextNumTimesPlayed, m_nextVariationForPain, m_nextNextTimeToLoadPain, isJumpingOrRwnd, true);
 	else if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
 		g_SpeechManager.PlayedPainForReplay(m_painVoiceType, m_painType, m_prevNumTimesPlayed, m_prevVariationForPain, m_prevNextTimeToLoadPain, isJumpingOrRwnd, false);

	//
// 	if(((eventInfo.GetPlaybackFlags() & REPLAY_CURSOR_JUMP) && (eventInfo.GetPlaybackFlags() & REPLAY_DIRECTION_BACK))/*|| (eventInfo.GetPlaybackFlags() & REPLAY_DIRECTION_BACK)*/)
// 		g_SpeechManager.SetNeedToLoadPlayerBank(false);
}

//////////////////////////////////////////////////////////////////////////
CPacketPainWaveBankChange::CPacketPainWaveBankChange(u8 prevBank, u8 nextBank)
	: CPacketEvent(PACKETID_SND_PAINWAVEBANKCHANGE, sizeof(CPacketPainWaveBankChange))
	, CPacketEntityInfo()
	, m_previousBank(prevBank)
	, m_nextBank(nextBank)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketPainWaveBankChange::Extract(const CEventInfo<void>& eventInfo) const
{
	if(	(eventInfo.GetPlaybackFlags().GetState() & REPLAY_CURSOR_JUMP) || 
		(eventInfo.GetPlaybackFlags().GetState() & REPLAY_STATE_PAUSE) ||
		(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK))
	{
		//g_SpeechManager.SetReplayPainBanks(m_previousBank, m_nextBank);
 		if((eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK) /*&& g_SpeechManager.GetReplayPainBank() != m_previousBank*/)
 		{	replayDebugf1("Setting wave bank backwards %u", m_previousBank);
 				g_SpeechManager.SetReplayPainBanks(m_nextBank, m_previousBank, true);
 		}
 		else if((eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD) /*&& g_SpeechManager.GetReplayPainBank() != m_nextBank*/)
 		{	replayDebugf1("Setting wave bank forewards");
 				g_SpeechManager.SetReplayPainBanks(m_previousBank, m_nextBank, false);
 		}
// 		else
// 		{
// 			g_SpeechManager.SetReplayPainBank(g_SpeechManager.GetBankNumForPain(AUD_PAIN_VOICE_TYPE_PLAYER), false);
// 			g_SpeechManager.SetReplayPainBank(g_SpeechManager.GetBankNumForPain(AUD_PAIN_VOICE_TYPE_PLAYER), false);
// 		}
	}
// 	else if((eventInfo.GetPlaybackFlags() & REPLAY_DIRECTION_BACK))
// 	{
// 		g_SpeechManager.SetBankNumForPain(AUD_PAIN_VOICE_TYPE_PLAYER, m_previousBank);
// 	}
}


//////////////////////////////////////////////////////////////////////////
CPacketSetPlayerPainRoot::CPacketSetPlayerPainRoot(const char* pPrev, const char* pNext)
	: CPacketEvent(PACKETID_SND_SETPLAYERPAINROOT, sizeof(CPacketSetPlayerPainRoot))
	, CPacketEntityInfo()
{
	formatf(m_prevRoot, sizeof(m_prevRoot), pPrev);
	formatf(m_nextRoot, sizeof(m_nextRoot), pNext);
}

//////////////////////////////////////////////////////////////////////////
void CPacketSetPlayerPainRoot::Extract(const CEventInfo<void>& /*eventInfo*/) const
{
// 	if(eventInfo.GetPlaybackFlags() & REPLAY_DIRECTION_FWD)
// 		g_SpeechManager.SetPlayerPainRootBankName(m_nextRoot);
// 	else if(eventInfo.GetPlaybackFlags() & REPLAY_DIRECTION_BACK)
// 		g_SpeechManager.SetPlayerPainRootBankName(m_prevRoot);
// 	else
// 	{
// 		replayFatalAssertf(false, "Invalid playback flags");
// 	}
}


CPacketSpeechStop::CPacketSpeechStop(s16 uStopType)
	: CPacketEvent(PACKETID_SOUND_SPEECH_STOP, sizeof(CPacketSpeechStop))
	, CPacketEntityInfo()
	, m_stopType(uStopType)
{}

void CPacketSpeechStop::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();

	audSpeechAudioEntity* pSpeechAudioEntity = NULL;
	
	if(pPed) 
	{
		pSpeechAudioEntity = pPed->GetSpeechAudioEntity();
	}
	else
	{
		pSpeechAudioEntity = &g_ScriptAudioEntity.GetSpeechAudioEntity();
	}

	if(pSpeechAudioEntity)
	{
		switch(m_stopType)
		{
		case SPEECH_STOP_AMBIENT:
			pSpeechAudioEntity->StopAmbientSpeech();
			break;
		case SPEECH_STOP_SCRIPTED:
			pSpeechAudioEntity->StopPlayingScriptedSpeech();
			g_ScriptAudioEntity.RemoveFromAreadyPlayedList(pSpeechAudioEntity->GetCurrentlyPlayingVoiceNameHash(), pSpeechAudioEntity->GetCurrentlyPlayingAmbientSpeechContextHash(), pSpeechAudioEntity->GetLastVariationNumber());
			break;
		case SPEECH_STOP_ALL:
			pSpeechAudioEntity->StopSpeech();
			break;
		case SPEECH_STOP_PAIN:
			pSpeechAudioEntity->StopPain();
			break;
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion g_PacketScriptedSpeech_v1 = 1;
CPacketScriptedSpeech::CPacketScriptedSpeech(u32 uContextHash, u32 uContextNameHash, u32 uVoiceNameHash, s32 sPreDelay, audSpeechVolume uRequestedVolume, audConversationAudibility uAudibility, s32 uVariationNumber, u32 sWaveSlotIndex, bool activeSpeechIsHeadset, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo, bool isUrgent, bool playSpeechWhenFinishedPreparing)
	: CPacketEvent(PACKETID_SOUND_SPEECH, sizeof(CPacketScriptedSpeech), g_PacketScriptedSpeech_v1)
	, CPacketEntityInfo()
{
	m_unused0 = 0;
	m_uContextHash = uContextHash;
	m_uContextNameHash = uContextNameHash;

	m_uVoiceNameHash = uVoiceNameHash;
	m_sPreDelay = sPreDelay;

	m_uRequestedVolume = uRequestedVolume;
	m_uAudibility = uAudibility;
	m_uVariationNumber = uVariationNumber;
	m_sWaveSlotIndex = sWaveSlotIndex;
	m_ActiveSpeechIsHeadset = activeSpeechIsHeadset;

	m_IsPitchedDuringSlowMo = isPitchedDuringSlowMo;
	m_IsMutedDuringSlowMo = isMutedDuringSlowMo;

	m_IsUrgent = isUrgent;

	m_PlaySpeechWhenFinishedPreparing = playSpeechWhenFinishedPreparing;
}

void CPacketScriptedSpeech::Extract(const CEventInfo<CPed>& eventInfo) const
{
	//Displayf("CPacketScriptedSpeech::Extract time:%d - %u - %d - %d", CReplayMgr::GetCurrentTimeRelativeMs(), m_uContextNameHash, m_sWaveSlotIndex, m_sPreDelay);
	if(!CReplayMgr::IsJustPlaying())
	{
		return; 
	}

	if(!g_ScriptAudioEntity.HasAlreadyPlayed(m_uVoiceNameHash, m_uContextHash, m_uVariationNumber)) 
	{
		Preload(eventInfo); // we're only going to preload. The Update packets will start the sound.
	}
}

ePreloadResult CPacketScriptedSpeech::Preload(const CEventInfo<CPed>& UNUSED_PARAM(eventInfo)) const
{
	return g_ScriptAudioEntity.PreloadReplayScriptedSpeech(m_uVoiceNameHash, m_uContextHash, m_uVariationNumber, m_sWaveSlotIndex);
}

tPacketVersion g_PacketScriptedSpeechUpdate_v1 = 1;
CPacketScriptedSpeechUpdate::CPacketScriptedSpeechUpdate(u32 uContextHash, u32 uContextNameHash, u32 uVoiceNameHash, audSpeechVolume uRequestedVolume, audConversationAudibility uAudibility,s32 uVariationNumber, s32 sStartOffset, u32 uStartTime, u32 uGameTime, u32 sWaveSlotIndex, bool activeSpeechIsHeadset, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo, bool isUrgent, bool playSpeechWhenFinishedPreparing)
	: CPacketEvent(PACKETID_SOUND_SPEECH_SCRIPTED_UPDATE, sizeof(CPacketScriptedSpeechUpdate), g_PacketScriptedSpeechUpdate_v1)
	, CPacketEntityInfo()
{
	m_unused0 = 0;
	m_uContextHash = uContextHash;
	m_uContextNameHash = uContextNameHash;
	m_uVoiceNameHash = uVoiceNameHash;

	m_uRequestedVolume = uRequestedVolume;
	m_uAudibility = uAudibility;
	m_uVariationNumber = uVariationNumber;

	m_uStartTime = uStartTime;
	m_uGameTime = uGameTime;
	m_sWaveSlotIndex = sWaveSlotIndex;
	m_sStartOffset = sStartOffset;

	m_ActiveSpeechIsHeadset = activeSpeechIsHeadset;
	m_IsPitchedDuringSlowMo = isPitchedDuringSlowMo;
	m_IsMutedDuringSlowMo = isMutedDuringSlowMo;

	m_IsUrgent = isUrgent;

	m_PlaySpeechWhenFinishedPreparing = playSpeechWhenFinishedPreparing;
}

void CPacketScriptedSpeechUpdate::Extract(const CEventInfo<CPed>& eventInfo) const
{
	//Displayf("CPacketScriptedSpeechUpdate::Extract time:%d - %u - %d - %d", CReplayMgr::GetCurrentTimeRelativeMs(), m_uContextNameHash, m_sWaveSlotIndex, m_sPreDelay);
	if(!CReplayMgr::IsJustPlaying())
	{
		return; 
	}

	if(Preload(eventInfo) == PRELOAD_SUCCESSFUL)
	{
		// If the startoffset is -2 then this indicates the actual play frame of the speech
		// -1 is preloading.
		if(!g_ScriptAudioEntity.HasAlreadyPlayed(m_uVoiceNameHash, m_uContextHash, m_uVariationNumber) && (m_sStartOffset > 0 || m_sStartOffset == -2)) // we have -1 start offset until it is supposed to start playing
		{
			s32 startOffset = m_sStartOffset;
			s32 preDelay = 0;
			if(m_sStartOffset < 66) // we kill very small start offsets because when playing back the complete line we may have recored a small offset instead of 0 at the start.
			{
				startOffset = 0;
			}
			bool playWhenFinishedPreparing = true;
			CPed* pPed = eventInfo.GetEntity();

			s32 waveSlot = g_ScriptAudioEntity.LookupReplayPreloadScriptedSpeechSlot(m_uVoiceNameHash, m_uContextHash, m_uVariationNumber);
			if(waveSlot == -1)
			{
				return;
			}
			
			if(GetPacketVersion() >= g_PacketScriptedSpeechUpdate_v1)
			{
				if(pPed && pPed->GetSpeechAudioEntity())
				{
					pPed->GetSpeechAudioEntity()->ReplayPlayScriptedSpeech( m_uContextHash, m_uContextNameHash, m_uVoiceNameHash, preDelay, m_uRequestedVolume,m_uAudibility, m_uVariationNumber, startOffset, waveSlot, m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, playWhenFinishedPreparing);//m_PlaySpeechWhenFinishedPreparing);
				}
				else
				{
					g_ScriptAudioEntity.GetSpeechAudioEntity().ReplayPlayScriptedSpeech( m_uContextHash, m_uContextNameHash, m_uVoiceNameHash, preDelay, m_uRequestedVolume,m_uAudibility, m_uVariationNumber, startOffset, waveSlot, m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, playWhenFinishedPreparing);
				}
			}
			else
			{
				//Old version
				if(pPed && pPed->GetSpeechAudioEntity())
				{
					pPed->GetSpeechAudioEntity()->ReplayPlayScriptedSpeech( m_uContextHash, m_uContextNameHash, m_uVoiceNameHash, preDelay, m_uRequestedVolume, AUD_AUDIBILITY_NORMAL,m_uVariationNumber, startOffset, waveSlot, m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, playWhenFinishedPreparing);//m_PlaySpeechWhenFinishedPreparing);
				}
				else
				{
					g_ScriptAudioEntity.GetSpeechAudioEntity().ReplayPlayScriptedSpeech( m_uContextHash, m_uContextNameHash, m_uVoiceNameHash, preDelay, m_uRequestedVolume, AUD_AUDIBILITY_NORMAL, m_uVariationNumber, startOffset, waveSlot, m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, playWhenFinishedPreparing);
				}
			}
		}
		else if(m_sStartOffset > 0)
		{
			g_ScriptAudioEntity.UpdateTimeStamp(m_uVoiceNameHash, m_uContextHash, m_uVariationNumber);
		}

	}
}

ePreloadResult CPacketScriptedSpeechUpdate::Preload(const CEventInfo<CPed>& UNUSED_PARAM(eventInfo)) const
{
	//Displayf("CPacketScriptedSpeechUpdate::Preload time:%d - %u - %d - %d", CReplayMgr::GetCurrentTimeRelativeMs(), m_uContextNameHash, m_sWaveSlotIndex, m_sPreDelay);

	//Displayf("CPacketScriptedSpeechUpdate::Preload time:%u - %u - %d - evenet:%u, %u, %u game:%u", CReplayMgr::GetCurrentTimeRelativeMs(), m_uContextNameHash, m_sWaveSlotIndex, 
	//	eventInfo.GetReplayTime(), eventInfo.GetPreloadOffsetTime(), eventInfo.GetIsFirstFrame(),
	//	GetGameTime());

	return g_ScriptAudioEntity.PreloadReplayScriptedSpeech(m_uVoiceNameHash, m_uContextHash, m_uVariationNumber, m_sWaveSlotIndex);
}


// Unused Packet
CPacketPlayPreloadedSpeech::CPacketPlayPreloadedSpeech(s32 timeOfPredelay)
	: CPacketEvent(PACKETID_SOUND_PLAYPRELOADEDSPEECH, sizeof(CPacketPlayPreloadedSpeech))
	, CPacketEntityInfo()
	, m_timeOfPredelay(timeOfPredelay)
{}

void CPacketPlayPreloadedSpeech::Extract(const CEventInfo<CPed>& /*eventInfo*/) const
{
	(void)m_timeOfPredelay;
	// Doesn't do anything - Unused packet
}


//////////////////////////////////////////////////////////////////////////
CPacketAnimalVocalization::CPacketAnimalVocalization(u32 contextPHash)
: CPacketEvent(PACKETID_SOUND_SPEECH_ANIMAL, sizeof(CPacketAnimalVocalization))
, CPacketEntityInfo()
{
	m_uContextHash = contextPHash;
}

//////////////////////////////////////////////////////////////////////////
void CPacketAnimalVocalization::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = (CPed*)eventInfo.GetEntity(0);

	if(pPed)
	{
		pPed->GetSpeechAudioEntity()->PlayAnimalVocalization(m_uContextHash);
	}
}

#endif // GTA_REPLAY
