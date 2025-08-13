#ifndef CUTSCENE_AUDIO_PACKET_H_
#define CUTSCENE_AUDIO_PACKET_H_

#include "Control/replay/ReplayController.h"

#if GTA_REPLAY

#include "Control/Replay/Audio/SoundPacket.h"
#include "audio/speechaudioentity.h"

//////////////////////////////////////////////////////////////////////////
class CCutSceneAudioPacket : public CPacketEvent, public CPacketEntityInfo<0>
{
public:

	CCutSceneAudioPacket(u32 trackTime, u32 startTime);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>& eventInfo ) const;
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CUTSCENE, "Validation of CCutSceneAudioPacket Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CUTSCENE;
	}

private:

	u32 m_CutFileHash;
	u32 m_TrackTime;
	u32 m_Startime;
};

//////////////////////////////////////////////////////////////////////////
class CCutSceneStopAudioPacket : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CCutSceneStopAudioPacket(bool isSkipping, u32 releaseTime);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_CUTSCENE_STOP, "Validation of CCutSceneStopAudioPacket Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_CUTSCENE_STOP;
	}
private:

	bool m_IsSkipping;
	u32 m_ReleaseTime;
};


//////////////////////////////////////////////////////////////////////////
class CSynchSceneAudioPacket : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CSynchSceneAudioPacket(u32 hash, const char* sceneName, u32 sceneTime, bool usePosition, bool useEntity, Vector3 pos);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>& eventInfo) const;
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SYNCHSCENE, "Validation of CSynchedSceneAudioPacket Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SYNCHSCENE;
	}

private:

	u32 m_Hash;
	u32 m_SceneTime;
	CPacketVector3 m_Position;
	char m_SceneName[64];
	bool m_UsePosition : 1;
	bool m_UseEntity : 1;
};

//////////////////////////////////////////////////////////////////////////
class CSynchSceneStopAudioPacket : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CSynchSceneStopAudioPacket(u32 hash);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SOUND_SYNCHSCENE_STOP, "Validation of CSynchedSceneStopAudioPacket Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_SYNCHSCENE_STOP;
	}
private:

	u32 m_Hash;
};

#endif // GTA_REPLAY

#endif // CUTSCENE_AUDIO_PACKET_H_
