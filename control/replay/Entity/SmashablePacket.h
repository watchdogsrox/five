//=====================================================================================================
// name:		SmashablePacket.h
// description:	Smashable replay packet.
//=====================================================================================================

#ifndef INC_SMASHABLEPACKET_H_
#define INC_SMASHABLEPACKET_H_

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"


#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS


class CEntity;
namespace rage
{
	class Vec3V;
	class Vector3;
}

//////////////////////////////////////////////////////////////////////////
class CPacketSmashableExplosion : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketSmashableExplosion(CEntity* pEntity, Vec3V& rPos, float fRadius, float fForce, bool bRemoveDetachedPolys);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	ShouldInvalidate() const		{	return false;	}
	void	Invalidate()	{ 	}

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SMASHABLEEXPLOSION, "Validation of CPacketSmashableExplosion Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SMASHABLEEXPLOSION;
	}

protected:

	CPacketVector3		m_Position;

	union
	{
		struct
		{
			u16	m_uCompID;
			s8	m_uNormalXY[2];
		};
		float m_fRadius;
	};

	float				m_fForce;

	u8					m_uWeaponGrp			:5;
	u8					m_bRemoveDetachedPolys	:1;
	u8					m_bIsBloody				:1;
	u8					m_bIsWater				:1;

	u16					m_uModelIndex;
};


//////////////////////////////////////////////////////////////////////////
class CPacketSmashableCollision : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketSmashableCollision(CEntity* pEntity, Vec3V& rPos, Vec3V& rNormal, Vec3V& rDirection, u32 uMaterialID, s32 uCompID, u8 uWeaponGrp, float fForce, bool bIsBloody, bool bIsWater, bool bIsHeli);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	ShouldInvalidate() const		{	return false;	}
	void	Invalidate()	{ 	}

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SMASHABLECOLLISION, "Validation of CPacketSmashableCollision Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SMASHABLECOLLISION;
	}

protected:

	CPacketVector3		m_Position;
	CPacketVector3Comp<valComp<s8, s8_f32_n1_to_1>>	m_direction;
	CPacketVector3Comp<valComp<s8, s8_f32_n1_to_1>>	m_normal;

	u8					m_Padding;

	u32					m_uMaterialID;

	u16					m_uCompID;

	float				m_fForce;

	u8					m_uWeaponGrp			:5;
	u8					m_bRemoveDetachedPolys	:1;
	u8					m_bIsBloody				:1;
	u8					m_bIsWater				:1;

	u16					m_uModelIndex;
};

#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // INC_SMASHABLEPACKET_H_
