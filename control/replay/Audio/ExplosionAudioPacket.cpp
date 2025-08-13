//=====================================================================================================
// name:		ExplosionAudioPacket.cpp
//=====================================================================================================

#include "ExplosionAudioPacket.h"

#if GTA_REPLAY

#include "audio/explosionaudioentity.h"
#include "control/replay/replay.h"
#include "scene/Entity.h"

tPacketVersion g_PacketExplosion_v1 = 1;
//////////////////////////////////////////////////////////////////////////
CPacketExplosion::CPacketExplosion(eExplosionTag tag, const Vector3& position, bool isUnderwater, fwInteriorLocation interiorLocation, u32 weaponHash)
	: CPacketEvent(PACKETID_SOUND_EXPLOSION, sizeof(CPacketExplosion), g_PacketExplosion_v1)
	, CPacketEntityInfo()
	, m_tag((u8)tag)
	, m_isUnderwater(isUnderwater)
	, m_position(position)
	, m_interiorLocation(interiorLocation)
	, m_weaponHash(weaponHash)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketExplosion::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CEntity* pEntity = eventInfo.GetEntity();
	if(GetPacketVersion() >= g_PacketExplosion_v1)	
	{
		g_audExplosionEntity.ReallyPlayExplosion(Tag(), m_position, pEntity, IsUnderwater(), m_interiorLocation, m_weaponHash);
	}
	else
	{
		g_audExplosionEntity.ReallyPlayExplosion(Tag(), m_position, pEntity, IsUnderwater(), m_interiorLocation, 0);
	}
}


#endif // GTA_REPLAY
