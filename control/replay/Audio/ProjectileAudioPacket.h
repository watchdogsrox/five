//=====================================================================================================
// name:		WeaponAudioPacket.h
// description:	Projectile audio replay packet.
//=====================================================================================================

#ifndef INC_PROJECTILEAUDIOPACKET_H_
#define INC_PROJECTILEAUDIOPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/effects/ParticlePacket.h"

//=====================================================================================================
class CPacketProjectile : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketProjectile();

	void InvalidatePacket()					{ m_Invalid = true; }

	/*TODO4FIVE void Store(CObject* pObject, eWeaponType WeaponType, u8 Projectileindex, bool isUnderWater = false, bool bForceStop = false );*/
	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ShouldInvalidate() const			{	return false;	}
	void Invalidate()						{ 	}
	bool ValidatePacket() const	
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_PROJECTILE, "Validation of CPacketProjectile Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PROJECTILE;
	}

	void Print() const						{}
	void PrintXML(FileHandle) const	{ }

private:
	u8					m_Invalid;

	u8					m_bIsUnderWater;
	u8					m_ProjectileIndex;

	/*TODO4FIVE eWeaponType			m_eWeaponType;*/

	u8					m_bForceStop;
};

class CPacketProjectileStop : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketProjectileStop();

	void InvalidatePacket()					{ m_Invalid = true; }

	/*void Store( u8 Projectileindex );*/
	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ShouldInvalidate() const			{	return false;	}
	void Invalidate()						{ 	}
	bool ValidatePacket() const	
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_PROJECTILESTOP, "Validation of CPacketProjectileStop Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PROJECTILESTOP;
	}

	void Print() const						{}
	void PrintXML(FileHandle) const	{ }

private:
	u8					m_Invalid;
	u8					m_ProjectileIndex;
};


#endif // GTA_REPLAY

#endif 
