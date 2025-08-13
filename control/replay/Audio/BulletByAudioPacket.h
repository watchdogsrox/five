//=====================================================================================================
// name:		BulletByAudioPacket.h
// description:	Bullet wizzes by  audio replay packet.
//=====================================================================================================

#ifndef INC_BULLETBYAUDIOPACKET_H_
#define INC_BULLETBYAUDIOPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Audio/SoundPacket.h"

namespace rage
{ class Vector3; }

//////////////////////////////////////////////////////////////////////////
class CPacketSoundBulletBy : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSoundBulletBy(const Vector3 &vecStart, const Vector3 &vecEnd, const s32 predelay, const u32 weaponAudioHash);

	bool		Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL;}
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	void		PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_BULLETBY, "Validation of CPacketSoundBulletBy Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_BULLETBY;
	}

private:
	CPacketVector3	m_vecStart;
	CPacketVector3	m_vecEnd;
	s32				m_preDelay;

	//Version 1
	u32				m_weaponAudioHash;
};

//////////////////////////////////////////////////////////////////////////
class CPacketSoundExplosionBulletBy : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSoundExplosionBulletBy(const Vector3& vecStart);

	bool		Extract(const CEventInfo<void>&) const;
	ePreloadResult		Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const				{}
	void		PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_EXPLOSIONBULLETBY, "Validation of CPacketSoundExplosionBulletBy Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_EXPLOSIONBULLETBY;
	}

private:
	CPacketVector3	m_vecStart;
};

#endif // GTA_REPLAY

#endif // INC_PFXPACKET_H_
