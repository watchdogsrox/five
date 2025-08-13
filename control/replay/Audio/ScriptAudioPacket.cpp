//=====================================================================================================
// name:		ScriptAudioPacket.cpp
//=====================================================================================================

#include "ScriptAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "audiohardware/waveslot.h"
#include "audio/northaudioengine.h"


////////////////////////////////////////////////////////////////////////
CPacketSndLoadScriptWaveBank::CPacketSndLoadScriptWaveBank(u32 slotIndex, u32 bankNameHash)
	: CPacketEvent(PACKETID_SND_LOADSCRIPTWAVEBANK, sizeof(CPacketSndLoadScriptWaveBank))
	, CPacketEntityInfo()
{
	m_SlotIndex = slotIndex;
	m_BankNameHash = bankNameHash;
}

bool CPacketSndLoadScriptWaveBank::HasExpired(const CEventInfoBase& UNUSED_PARAM(eventInfo)) const
{
	return true;
}

void CPacketSndLoadScriptWaveBank::Extract(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{
}

ePreloadResult CPacketSndLoadScriptWaveBank::Preload(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{ 
	return PRELOAD_SUCCESSFUL;
}


////////////////////////////////////////////////////////////////////////
int CPacketSndLoadScriptBank::expirePrevious = -1;
CPacketSndLoadScriptBank::CPacketSndLoadScriptBank()
	: CPacketEvent(PACKETID_SND_LOADSCRIPTBANK, sizeof(CPacketSndLoadScriptBank))
	, CPacketEntityInfo()
{
	for(u32 loop = 0; loop < AUD_USABLE_SCRIPT_BANK_SLOTS; loop++)
	{
		audScriptBankSlot* bankSlot = g_ScriptAudioEntity.GetBankSlot(loop);
		if(bankSlot->ReferenceCount > 0 && bankSlot->BankId != 0xffff && bankSlot->BankNameHash != g_NullSoundHash)
		{
			m_LoadedBanks[loop] = bankSlot->BankNameHash;
		}		
		else
		{
			m_LoadedBanks[loop] = g_NullSoundHash;
		}
	}
}

void CPacketSndLoadScriptBank::Extract(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	for(u32 loop = 0; loop < AUD_USABLE_SCRIPT_BANK_SLOTS; loop++)
	{
		if(m_LoadedBanks[loop] != g_NullSoundHash)
		{
			g_ScriptAudioEntity.ReplayLoadScriptBank(m_LoadedBanks[loop], loop);
		}				
		else
		{
			audScriptBankSlot* bankScriptSlot = g_ScriptAudioEntity.GetBankSlot(loop);
			if(bankScriptSlot->ReferenceCount != 0)
			{
				g_ScriptAudioEntity.ReplayFreeScriptBank(loop);
			}
		}
	}
}

ePreloadResult CPacketSndLoadScriptBank::Preload(const CEventInfo<void>&) const		
{ 
	return PRELOAD_SUCCESSFUL; 
}

//////////////////////////////////////////////////////////////////////////
CPacketScriptSetVarOnSound::CPacketScriptSetVarOnSound(const u32 nameHash[], const float value[]) 
	: CPacketEventTracked(PACKETID_SOUND_SET_VARIABLE_ON_SOUND, sizeof(CPacketScriptSetVarOnSound))
	, CPacketEntityInfo()
{
	for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
	{
		m_VariableNameHash[i] = nameHash[i];
		m_Value[i] = value[i];
	}
}

//---------------------------------------------------------
// Func: CPacketScriptSetVarOnSound::Extract
//---------------------------------------------------------
void CPacketScriptSetVarOnSound::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if(pSound != NULL)
	{
		for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
		{
			if(m_VariableNameHash[i] != g_NullSoundHash)
				pSound->SetVariableValueDownHierarchyFromName(m_VariableNameHash[i], m_Value[i]);

		}
		return;
	}
	replayAssert(false && "No sound to set variable value on");
}

////////////////////////////////////////////////////////////////////////
CPacketScriptAudioFlags::CPacketScriptAudioFlags(const audScriptAudioFlags::FlagId flagId, const bool state)
	: CPacketEvent(PACKETID_SET_SCRIPT_AUDIO_FLAGS, sizeof(CPacketScriptAudioFlags))
	, CPacketEntityInfo()
{
	m_FlagId = flagId;
	m_State = state;
}

void CPacketScriptAudioFlags::Extract(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	g_ScriptAudioEntity.SetFlagState(m_FlagId,m_State);
}


CPacketScriptAudioSpecialEffectMode::CPacketScriptAudioSpecialEffectMode(const audSpecialEffectMode mode)
	: CPacketEvent(PACKETID_SOUND_SPECIAL_EFFECT_MODE, sizeof(CPacketScriptAudioSpecialEffectMode))
	, CPacketEntityInfo()
{
	m_Mode = mode;
}

void CPacketScriptAudioSpecialEffectMode::Extract(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{
	audNorthAudioEngine::GetGtaEnvironment()->SetScriptedSpecialEffectMode((audSpecialEffectMode)m_Mode);		
}



#endif // GTA_REPLAY
