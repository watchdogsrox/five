//=====================================================================================================
// name:		MeleeAudioPacket.h
// description:	Melee audio replay packet.
//=====================================================================================================

#ifndef INC_MELEEPACKET_H_
#define INC_MELEEPACKET_H_

#include "control\replay\ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/effects/ParticlePacket.h"
#include "Peds/ped.h"

//////////////////////////////////////////////////////////////////////////
class CPacketMelee : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketMelee(u32 uMeleeCombatObjectNameHash, bool bHasHitPed, const Vector3& vPosition, f32 fHarnessVolumeOffset, u32 weaponHash );

	u32  GetMeleeCombatObjectNameHash()		{ return m_MeleeCombatObjectNameHash; }

	void Extract(const CEventInfo<CPed>& eventInfo) const;

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_MELEE, "Validation of CPacketMelee Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_MELEE;
	}

protected:
	bool				HasHitPed() const	{	return m_hitPed;	}

	u32					m_WeaponHash;

	u32					m_MeleeCombatObjectNameHash;
	f32					m_HarnessVolumeOffset;
	CPacketVector<3>	m_Position;

	bool				m_hitPed;

};

#endif // GTA_REPLAY

#endif 
