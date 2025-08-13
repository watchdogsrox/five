//=====================================================================================================
// name:		ScriptAudioPacket.h
// description:	Script audio replay packet.
//=====================================================================================================

#ifndef INC_SCRIPTAUDIOPACKET_H_
#define INC_SCRIPTAUDIOPACKET_H_

#include "audio/scriptaudioentity.h"
#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/audio/SoundPacket.h"
#include "audio/scriptaudioentity.h"

class CPacketSndLoadScriptWaveBank  : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSndLoadScriptWaveBank(u32 slotIndex, u32 bankNameHash);
	
	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const;
	ePreplayResult		Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfoBase&) const;
	
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		//Displayf("Validate CPacketSndLoadScriptBank");
		replayAssertf(GetPacketID() == PACKETID_SND_LOADSCRIPTWAVEBANK, "Validation of CPacketSndLoadScriptWaveBank Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_LOADSCRIPTWAVEBANK;
	}

private:
	u32 m_BankNameHash;
	u32 m_SlotIndex;
};


class CPacketSndLoadScriptBank  : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSndLoadScriptBank();

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const;
	ePreplayResult		Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfoBase&) const     { return true; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SND_LOADSCRIPTBANK, "Validation of CPacketSndLoadScriptBank Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SND_LOADSCRIPTBANK;
	}

	static int expirePrevious;

private:
	u32 m_LoadedBanks[AUD_MAX_SCRIPT_BANK_SLOTS];
};


//---------------------------------------------------------
// Class: CPacketScriptSetVarOnSound
//---------------------------------------------------------
class CPacketScriptSetVarOnSound : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketScriptSetVarOnSound(const u32 hashName[], const float value[]);

	void			Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSoundType>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSoundType>&) const					{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SET_VARIABLE_ON_SOUND, "Validation of CPacketScriptSetVarOnSound Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SET_VARIABLE_ON_SOUND;
	}

private:
	u32		m_VariableNameHash[2];
	f32		m_Value[2];
};

class CPacketScriptAudioFlags : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketScriptAudioFlags(const audScriptAudioFlags::FlagId flagId, const bool state);

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const					{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfoBase&) const     { return true; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SET_SCRIPT_AUDIO_FLAGS, "Validation of CPacketScriptAudioFlags Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SET_SCRIPT_AUDIO_FLAGS;
	}

private:
	audScriptAudioFlags::FlagId m_FlagId;
	bool m_State;
};

class CPacketScriptAudioSpecialEffectMode : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketScriptAudioSpecialEffectMode(const audSpecialEffectMode mode);

	void				Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<void>&) const					{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfoBase&) const     { return true; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_SPECIAL_EFFECT_MODE, "Validation of CPacketScriptAudioSpecialEffectMode Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SPECIAL_EFFECT_MODE;
	}

private:
	audSpecialEffectMode m_Mode;
};


#endif // GTA_REPLAY

#endif // INC_PFXPACKET_H_
