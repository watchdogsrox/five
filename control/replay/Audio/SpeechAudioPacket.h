//=====================================================================================================
// name:		SpeechAudioPacket.h
// description:	Speech audio replay packet.
//=====================================================================================================

#ifndef INC_SPEECHAUDIOPACKET_H_
#define INC_SPEECHAUDIOPACKET_H_

#include "Control/replay/ReplayController.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "Control/Replay/Audio/SoundPacket.h"
#include "audio/gameobjects.h"
#include "audio/speechaudioentity.h"
#include "audio/scriptaudioentity.h"

enum {
	SPEECH_STOP_AMBIENT = 0,
	SPEECH_STOP_SCRIPTED,
	SPEECH_STOP_ALL,
	SPEECH_STOP_PAIN,
};

//=====================================================================================================

//////////////////////////////////////////////////////////////////////////
class CPacketSpeech : public CPacketEvent, public CPacketEntityInfoStaticAware<2, CEntityCheckerAllowNoEntity, CEntityCheckerAllowNoEntity>
{
public:
	CPacketSpeech(const audDeferredSayStruct* m_DeferredSayStruct, bool checkSlotVancant = true, const Vector3 positionForNullSpeaker = ORIGIN, s32 startOffset = -1);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SPEECH_SAY, "Validation of CPacketSpeech Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SPEECH_SAY;
	}

private:

	u32 m_contextHash;
	u32 m_voiceNameHash;
	u32 m_requestedVolume;
	u32 m_variationNum;
	s32 m_offsetMs;
	s32 m_preDelay;
	f32 m_priority;
	SpeechAudibilityType m_requestedAudibility;
	bool m_preloadOnly;
	bool m_checkSlotVacant;
	Vector3 m_positionForNullSpeaker;
	bool m_IsPitchedDuringSlowMo;
	bool m_IsMutedDuringSlowMo;
};



//////////////////////////////////////////////////////////////////////////
class CPacketSpeechPain : public CPacketEvent, public CPacketEntityInfoStaticAware<2, CEntityChecker, CEntityCheckerAllowNoEntity>
{
public:
	CPacketSpeechPain(u8 painLevel, const audDamageStats& damageStats, f32 distToListener, u8 painVoiceType, u8 painType, u8 prevVariation, u8 prevNumTimes, u32 prevNextTimeToLoad, u8 nextVariation, u8 nextNumTimes, u32 nextNextTimeToLoad, u32 lastPainLevel, u32* lastPainTimes);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SPEECH_PAIN, "Validation of CPacketSpeechPain Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SPEECH_PAIN;
	}

	void	PrintXML(FileHandle handle) const
	{
		CPacketEvent::PrintXML(handle);

		char str[1024];
		snprintf(str, 1024, "\t\t<m_prevVariationForPain>%d</m_prevVariationForPain>\n", m_prevVariationForPain);
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<m_prevNumTimesPlayed>%d</m_prevNumTimesPlayed>\n", m_prevNumTimesPlayed);
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<m_prevNextTimeToLoadPain>%d</m_prevNextTimeToLoadPain>\n", m_prevNextTimeToLoadPain);
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<m_nextVariationForPain>%d</m_nextVariationForPain>\n", m_nextVariationForPain);
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<m_nextNumTimesPlayed>%d</m_nextNumTimesPlayed>\n", m_nextNumTimesPlayed);
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<m_nextNextTimeToLoadPain>%d</m_nextNextTimeToLoadPain>\n", m_nextNextTimeToLoadPain);
		CFileMgr::Write(handle, str, istrlen(str));
	}

private:
	f32		m_rawDamage;
	s32		m_preDelay;

	u8		m_damageReason		: 5;	// Only goes up to 29
	bool	m_fatal				: 1;
	bool	m_pedWasAlreadyDead	: 1;
	bool	m_playGargle		: 1;

	bool	m_isHeadshot		: 1;
	bool	m_isSilenced		: 1;
	bool	m_isSniper			: 1;
	bool	m_isFrontend		: 1;
	bool	m_isFromAnim		: 1;
	bool	m_forceDeathShort	: 1;
	bool	m_forceDeathMedium	: 1;
	bool	m_forceDeathLong	: 1;

	f32		m_distToListener;
	u8		m_painLevel;

	u8		m_painVoiceType;
	u8		m_painType;
	u8		m_prevVariationForPain;
	u8		m_prevNumTimesPlayed;
	u32		m_prevNextTimeToLoadPain;
	u8		m_nextVariationForPain;
	u8		m_nextNumTimesPlayed;
	u32		m_nextNextTimeToLoadPain;

	u32		m_lastPainLevel;
	u32		m_lastPainTimes[2];
};

//////////////////////////////////////////////////////////////////////////
class CPacketPainWaveBankChange : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketPainWaveBankChange(u8 prevBank, u8 nextBank);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SND_PAINWAVEBANKCHANGE, "Validation of CPacketPainWaveBankChange Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_PAINWAVEBANKCHANGE;
	}

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const		
	{	
		if( flags & REPLAY_STATE_PAUSE )
			return REPLAY_EXTRACT_SUCCESS;
		else
			return CPacketEvent::ShouldExtract(flags, packetFlags, isFirstFrame);
	}
private:
	
	u8		m_previousBank;
	u8		m_nextBank;
};


//////////////////////////////////////////////////////////////////////////
class CPacketSetPlayerPainRoot : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSetPlayerPainRoot(const char* pPrev, const char* pNext);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SND_SETPLAYERPAINROOT, "Validation of CPacketSetPlayerPainRoot Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_SETPLAYERPAINROOT;
	}

private:
	char	m_prevRoot[32];
	char	m_nextRoot[32];
};



class CPacketSpeechStop : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	//-------------------------------------------------------------------------------------------------
	CPacketSpeechStop(s16 uStopType);

	void	Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	ShouldEndTracking() const				{	return true;				}

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SPEECH_STOP, "Validation of CPacketSpeechStop Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SPEECH_STOP;
	}

private:
	s16		m_stopType;
};


class CPacketScriptedSpeech : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketScriptedSpeech(u32 uContextHash, u32 uContextNameHash, u32 uVoiceNameHash, s32 sPreDelay, audSpeechVolume uRequestedVolume, audConversationAudibility uAudibility, s32 uVariationNumber, u32 sWaveSlotIndex, bool activeSpeechIsHeadset, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo, bool isUrgent, bool playSpeechWhenFinishedPreparing);

	void	Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>& eventInfo) const;
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SPEECH, "Validation of CPacketScriptedSpeech Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SPEECH;
	}

private:	
	u32					m_uContextHash;
	u32					m_uVoiceNameHash;
	u32					m_uContextNameHash;
	s32					m_uVariationNumber;
	s32					m_sPreDelay;
	audSpeechVolume		m_uRequestedVolume;
	u32					m_sWaveSlotIndex;
	u8					m_ActiveSpeechIsHeadset				: 1;
	u8					m_IsPitchedDuringSlowMo				: 1;
	u8					m_IsMutedDuringSlowMo				: 1;
	u8					m_IsUrgent							: 1;
	u8					m_PlaySpeechWhenFinishedPreparing	: 1;
	u8					m_unused0							: 3;
	audConversationAudibility	m_uAudibility;
};


class CPacketScriptedSpeechUpdate : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	//-------------------------------------------------------------------------------------------------
	CPacketScriptedSpeechUpdate(u32 uContextHash, u32 uContextNameHash, u32 uVoiceNameHash, audSpeechVolume uRequestedVolume, audConversationAudibility uAudibility,s32 uVariationNumber, s32 sStartOffset, u32 uStartTime, u32 uGameTime, u32 sWaveSlotIndex, bool activeSpeechIsHeadset, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo, bool isUrgent, bool playSpeechWhenFinishedPreparing);

	void	Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const;
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SPEECH_SCRIPTED_UPDATE, "Validation of CPacketScriptedSpeechUpdate Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SPEECH_SCRIPTED_UPDATE;
	}

	u32  GetGameTime() const			{ return m_uGameTime;	}
	u32	 GetStartTime() const			{ return m_uStartTime;	}
	u32  GetContext() const				{ return m_uContextHash;	}

	bool HasAlreadyPlayed() const		{ return g_ScriptAudioEntity.HasAlreadyPlayed(m_uVoiceNameHash, m_uContextHash, m_uVariationNumber); }

private:

	u32							m_uContextHash;
	u32							m_uVoiceNameHash;
	u32							m_uStartTime;
	u32							m_uGameTime;
	u32							m_uContextNameHash;
	s32							m_sStartOffset;
	s32							m_uVariationNumber;
	audSpeechVolume				m_uRequestedVolume;
	u32							m_sWaveSlotIndex;
	u8							m_ActiveSpeechIsHeadset				: 1;
	u8							m_IsPitchedDuringSlowMo				: 1;
	u8							m_IsMutedDuringSlowMo				: 1;
	u8							m_IsUrgent							: 1;
	u8							m_PlaySpeechWhenFinishedPreparing	: 1;
	u8							m_unused0							: 3;
	audConversationAudibility	m_uAudibility;
};


// Unused Packet
class CPacketPlayPreloadedSpeech : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketPlayPreloadedSpeech(s32 timeOfPredelay);

	void	Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_PLAYPRELOADEDSPEECH, "Validation of CPacketPlayPreloadedSpeech Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PLAYPRELOADEDSPEECH;
	}

private:	
	s32	 m_timeOfPredelay;		
};


class CPacketAnimalVocalization : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	//-------------------------------------------------------------------------------------------------
	CPacketAnimalVocalization(u32 contextPHash);

	void	Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SPEECH_ANIMAL, "Validation of CPacketAnimalVocalization Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SPEECH_ANIMAL;
	}

private:
	u32							m_uContextHash;
};

#endif // GTA_REPLAY

#endif // INC_PFXPACKET_H_
