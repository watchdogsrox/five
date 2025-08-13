//=====================================================================================================
// name:		MeleeAudioPacket.cpp
//=====================================================================================================

#include "MeleeAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/PacketBasics.h"

#include "audio/pedaudioentity.h"
#include "peds/ped.h"


//////////////////////////////////////////////////////////////////////////
CPacketMelee::CPacketMelee(u32 uMeleeCombatObjectNameHash, bool bHasHitPed, const Vector3& vPosition, f32 fHarnessVolumeOffset, u32 weaponHash )
	: CPacketEvent(PACKETID_SOUND_MELEE, sizeof(CPacketMelee))
	, CPacketEntityInfo()
{
	m_hitPed = bHasHitPed;
	m_MeleeCombatObjectNameHash = uMeleeCombatObjectNameHash;
	m_Position = vPosition;
	m_HarnessVolumeOffset = fHarnessVolumeOffset;
	m_WeaponHash = weaponHash;
}


//////////////////////////////////////////////////////////////////////////
void CPacketMelee::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	if(pPed == NULL)
		return;

	if(pPed)
	{
		//pPed->GetPedAudioEntity()->PlayReplayMeleeCombatHit( m_MeleeCombatObjectNameHash, HasHitPed(), m_Position, m_HarnessVolumeOffset, m_WeaponHash );
	}
}


#endif // GTA_REPLAY
