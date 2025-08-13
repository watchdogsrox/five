//=====================================================================================================
// name:		ProjectileAudioPacket.cpp
//=====================================================================================================

#include "ProjectileAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"

#include "audio/pedaudioentity.h"
#include "peds/ped.h"

//========================================================================================================================
CPacketProjectile::CPacketProjectile()
: CPacketEvent(PACKETID_SOUND_PROJECTILE, sizeof(CPacketProjectile))
{
	REPLAY_UNUSEAD_VAR(m_bIsUnderWater);
	REPLAY_UNUSEAD_VAR(m_ProjectileIndex);
	REPLAY_UNUSEAD_VAR(m_bForceStop);
	REPLAY_UNUSEAD_VAR(m_ProjectileIndex);
}


//========================================================================================================================
/*TODO4FIVE void CPacketProjectile::Store(CObject* pObject, eWeaponType WeaponType, u8 Projectileindex, bool isUnderWater, bool forceStop)
{
	m_PacketID = PACKETID_SOUND_PROJECTILE;
	m_GameTime = CTimer::GetReplayTimeInMilliseconds();
	m_Invalid = false;

	if( pObject )
	{
		m_EntityID = pObject->GetReplayID();
	}
	else
	{
		m_EntityID = -1;
		m_Invalid = true;
	}
	m_bIsUnderWater = isUnderWater;
	m_eWeaponType = WeaponType;
	m_ProjectileIndex = Projectileindex;
	m_bForceStop = forceStop;
}*/

void CPacketProjectile::Extract(const CEventInfo<void>&) const
{
	if( m_Invalid )
		return;

	CObject* pObject = NULL;
	/*TODO4FIVEif( m_EntityID != -1 )
		pObject = (CObject*)CReplayMgr::GetEntity( REPLAYIDTYPE_PLAYBACK, m_EntityID );*/

	if( pObject )
	{
		//g_WeaponAudioEntity.ReplayAttachProjectileLoop( m_eWeaponType, pObject, m_ProjectileIndex, m_bIsUnderWater, m_bForceStop );
	}

}

CPacketProjectileStop::CPacketProjectileStop()
: CPacketEvent(PACKETID_SOUND_PROJECTILESTOP, sizeof(CPacketProjectileStop))
{
	REPLAY_UNUSEAD_VAR(m_ProjectileIndex);
}

//========================================================================================================================
// void CPacketProjectileStop::Store(u8 Projectileindex)
// {
// 	m_PacketID = PACKETID_SOUND_PROJECTILESTOP;
// 	m_GameTime = fwTimer::GetReplayTimeInMilliseconds();
// 	m_Invalid = false;
// 	m_ProjectileIndex = Projectileindex;
// }

void CPacketProjectileStop::Extract(const CEventInfo<void>&) const
{
	if( m_Invalid )
		return;

	//g_WeaponAudioEntity.StopReplayAttachProjectileLoop( m_ProjectileIndex );
}

#endif // GTA_REPLAY
