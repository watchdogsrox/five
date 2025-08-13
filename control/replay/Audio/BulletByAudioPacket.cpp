//=====================================================================================================
// name:		BulletByAudioPacket.cpp
//=====================================================================================================

#include "BulletByAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY
 
#include "control/replay/ReplayInternal.h"
#include "audio/weaponaudioentity.h"

tPacketVersion g_PacketSoundBulletBy_v1 = 1;
//////////////////////////////////////////////////////////////////////////
CPacketSoundBulletBy::CPacketSoundBulletBy(const Vector3 &vecStart, const Vector3 &vecEnd, const s32 predelay, const u32 weaponAudioHash)
: CPacketEvent(PACKETID_SOUND_BULLETBY, sizeof(CPacketSoundBulletBy), g_PacketSoundBulletBy_v1)
{
	m_vecStart.Store(vecStart);
	m_vecEnd.Store(vecEnd);
	m_preDelay = predelay;
	m_weaponAudioHash = weaponAudioHash;
}

//////////////////////////////////////////////////////////////////////////
bool CPacketSoundBulletBy::Extract(const CEventInfo<void>&) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return false;
	}

	Vector3 vecStart;
	m_vecStart.Load(vecStart);
	Vector3 vecEnd;
	m_vecEnd.Load(vecEnd);

	if (GetPacketVersion() >= g_PacketSoundBulletBy_v1)
	{
		g_WeaponAudioEntity.PlayBulletBy(m_weaponAudioHash, vecStart, vecEnd, m_preDelay);
	}	
	else
	{
		g_WeaponAudioEntity.PlayBulletBy(0u, vecStart, vecEnd, m_preDelay);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundExplosionBulletBy::CPacketSoundExplosionBulletBy(const Vector3& vecStart)
: CPacketEvent(PACKETID_SOUND_EXPLOSIONBULLETBY, sizeof(CPacketSoundExplosionBulletBy))
{
	m_vecStart.Store(vecStart);
}

//////////////////////////////////////////////////////////////////////////
bool CPacketSoundExplosionBulletBy::Extract(const CEventInfo<void>&) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return false;
	}

	Vector3 vecStart;
	m_vecStart.Load(vecStart);

	g_WeaponAudioEntity.PlayExplosionBulletBy(vecStart);

	return true;
}


#endif // GTA_REPLAY