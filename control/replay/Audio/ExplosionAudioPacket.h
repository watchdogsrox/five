//=====================================================================================================
// name:		ExplosionAudioPacket.h
// description:	Explosion audio replay packet.
//=====================================================================================================

#ifndef INC_EXPLOSIONAUDIOPACKET_H_
#define INC_EXPLOSIONAUDIOPACKET_H_

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/effects/ParticlePacket.h"

#include "weapons/WeaponEnums.h"


//////////////////////////////////////////////////////////////////////////
class CPacketExplosion : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketExplosion(eExplosionTag tag, const Vector3& position, bool isUnderwater, fwInteriorLocation interiorLocation, u32 weaponHash);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult				Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_EXPLOSION, "Validation of CPacketExplosion Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_EXPLOSION;
	}

private:
	eExplosionTag		Tag() const					{	return static_cast<eExplosionTag>(m_tag);	}
	bool				IsUnderwater() const		{	return m_isUnderwater != 0;					}

	u8					m_tag			: 7;
	u8					m_isUnderwater	: 1;
	CPacketVector<3>	m_position;
	fwInteriorLocation	m_interiorLocation;
	u32					m_weaponHash;

};

#endif // GTA_REPLAY

#endif // INC_EXPLOSIONAUDIOPACKET_H_
