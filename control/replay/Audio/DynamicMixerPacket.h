
#ifndef INC_DYNAMICMIXER_PACKET_H_
#define INC_DYNAMICMIXER_PACKET_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "audioengine/dynamixdefs.h"
#include "control/replay/Audio/SoundPacket.h"

struct replaytrackedScene
{
	replaytrackedScene() { m_pScene = NULL; m_slot = -1; }
	replaytrackedScene(audScene* scene) { m_pScene = scene; m_slot = -1;}
	replaytrackedScene(audScene* scene, s32 slot) { m_pScene = scene; m_slot = slot;}
	replaytrackedScene(const replaytrackedScene& other) { m_pScene = other.m_pScene; m_slot = other.m_slot; }

	//void operator =(const audScene* scene) const { m_pScene = scene; }
	bool operator !=(audScene* scene) const { return m_pScene != scene; }
	bool operator ==(const replaytrackedScene& other) const { return m_pScene == other.m_pScene; }


	audScene* m_pScene;
	s32		  m_slot;
};

typedef replaytrackedScene tTrackedSceneType;

//////////////////////////////////////////////////////////////////////////
class CPacketDynamicMixerScene : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketDynamicMixerScene(const u32 hash, bool stop, u32 startOffset);

	bool			Extract(CTrackedEventInfo<tTrackedSceneType>&) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSceneType>&) const		{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedSceneType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool			HasExpired(const CTrackedEventInfo<tTrackedSceneType>&) const;

	bool	Setup(bool tracked)
	{
		if(!m_stop)
		{
			return true;
		}

		replayDebugf2("Stop (weapon) tracking %d", GetTrackedID());
		if(!tracked)
		{
			replayDebugf2("Sound not started so cancelling stop");
			Cancel();
		}
		else
		{
			replayDebugf2("Stop is fine");
		}
		return false;
	}

	bool			ShouldStartTracking() const										{ return !m_stop; }
	bool			ShouldEndTracking() const										{ return m_stop;	}

	void		PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DYNAMICMIXER_SCENE, "Validation of CPacketDynamicMixerScene Failed!, %d", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DYNAMICMIXER_SCENE;
	}

private:
	u32 m_Hash;
	bool m_stop;
	u32 m_StartOffset;
};

//---------------------------------------------------------
// Class: CPacketSceneSetVariable
// Desc:  History buffer packet holding information to
//         update the value of a scene variable.
//---------------------------------------------------------
class CPacketSceneSetVariable : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketSceneSetVariable(u32 nameHash, f32 value);

	void			Extract(CTrackedEventInfo<tTrackedSceneType>& pData) const;
	ePreloadResult	Preload(const CTrackedEventInfo<tTrackedSceneType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<tTrackedSceneType>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_SET_AUDIO_SCENE_VARIABLE, "Validation of CPacketSceneSetVariable Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SET_AUDIO_SCENE_VARIABLE;
	}

private:
	u32 m_NameHash;
	f32 m_Value;
};


#endif // GTA_REPLAY

#endif // INC_DYNAMICMIXER_PACKET_H_
