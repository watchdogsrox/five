//=====================================================================================================
// name:		ParticleEntityFxPacket.h
// description:	Entity Fx particle replay packet.
//=====================================================================================================

#ifndef INC_ENTITYFXPACKET_H_
#define INC_ENTITYFXPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/InterpPacket.h"
#include "control/replay/PacketBasics.h"
#include "control/replay/Effects/ParticlePacket.h"
#include "vector/vector3.h"
#include "vector/quaternion.h"

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

namespace rage
{class ptxEffectInst;}
class CEntity;

class CPacketEntityAmbientFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketEntityAmbientFx(const CParticleAttr* pPtFxAttr, s32 effectId, bool isTrigger);

	void Extract(const CEventInfo<CEntity>& eventInfo) const;

	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ENTITYAMBIENTFX, "Validation of CPacketEntityAmbientFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ENTITYAMBIENTFX;
	}

private:
	CPacketPositionAndQuaternion m_PosAndRot;
	u32	m_tagHash;

	s32		m_fxType;
	s32		m_boneTag;
	float	m_scale;
	s32		m_probability;
	u8		m_tintR;
	u8		m_tintG;
	u8		m_tintB;
	u8		m_tintA;

	u8	m_hasTint : 1;
	u8	m_ignoreDamageModel : 1;
	u8	m_playOnParent : 1;
	u8	m_onlyOnDamageModel : 1;
	u8	m_allowRubberBulletShotFx : 1;
	u8	m_allowElectricBulletShotFx : 1;
	u8	m_isTrigger : 1;
	u8	m_unused : 1;

	u16 m_effectId;

};

//=====================================================================================================
class CPacketEntityFragBreakFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketEntityFragBreakFx(u32 uPfxHash, const Vector3& rEffectPos, const Quaternion& rQuatRot, float fUserScale, u16 uBoneTag, bool bPlayOnParent, bool bHasTint, u8 uR, u8 uG, u8 uB);

	void Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ENTITYFRAGBREAKFX, "Validation of CPacketEntityFragBreakFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ENTITYFRAGBREAKFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StorePosition(const Vector3& rPosition);
	void LoadPosition(Vector3& rPosition) const;

	void StoreQuaternion(const Quaternion& rQuatRot);
	void LoadQuaternion(Quaternion& rQuatRot) const;

	//-------------------------------------------------------------------------------------------------
	float	m_Position[3];
	float	m_fUserScale;
	u32		m_pfxHash;
	u32		m_QuatRot;

	u16		m_uBoneTag;

	u8		m_Pad;
	u8		m_Tint[3];

	u8		m_bHasTint		:1;
	u8		m_bPlayOnParent	:1;
	u8		m_PadBits		:6;
};


//=====================================================================================================
class CPacketEntityShotFx_Old : public CPacketEvent, public CPacketEntityInfo<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketEntityShotFx_Old() : CPacketEvent(PACKETID_ENTITYSHOTFX_OLD, sizeof(CPacketEntityShotFx_Old))	{replayAssertf(false, "Old packet...shouldn't be called");}

	void Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ENTITYSHOTFX_OLD, "Validation of CPacketEntityShotFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ENTITYSHOTFX_OLD;
	}

protected:

	CPacketPositionAndQuaternion m_posAndQuat;
	u32 m_fxHashName;
	s32 m_boneIndex;
	bool m_atCollisionPoint;
	bool m_hasTint;
	float m_tintR, m_tintG, m_tintB, m_tintA;
	float m_scale;
};


//////////////////////////////////////////////////////////////////////////
class CPacketEntityShotFx : public CPacketEvent, public CPacketEntityInfoStaticAware<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketEntityShotFx(u32 fxHashName, s32 boneIndex, bool atCollisionPoint, const Mat34V_In& relFxMat, Vec3V_In vEffectPos, QuatV_In vEffectQuat, 
		bool hasTint, float tintR, float tintG, float tintB, float tintA, float scale );

	void Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ENTITYSHOTFX, "Validation of CPacketEntityShotFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_ENTITYSHOTFX;
	}

protected:

	CPacketPositionAndQuaternion m_posAndQuat;
	u32 m_fxHashName;
	s32 m_boneIndex;
	bool m_atCollisionPoint;
	bool m_hasTint;
	float m_tintR, m_tintG, m_tintB, m_tintA;
	float m_scale;
};


//////////////////////////////////////////////////////////////////////////
class CPacketPtFxFragmentDestroy : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketPtFxFragmentDestroy(u32 fxHashName, Mat34V_In &posAndRot, Vec3V_In &offsetPos, QuatV_In &offsetRot, float scale, bool hasTint, u8 tintR, u8 tintG, u8 tintB, u8 tintA);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTFX_FRAGMENTDESTROY, "Validation of CPacketEntityFragDestroyFx2 Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PTFX_FRAGMENTDESTROY;
	}

protected:

	u32								m_fxHashName;
	CPacketPositionAndQuaternion	m_posAndRot;
	CPacketVector3					m_OffsetPos;
	CPacketQuaternionL				m_OffsetRot;
	float							m_scale;
	bool							m_hasTint;
	u8								m_tintR;
	u8								m_tintG;
	u8								m_tintB;
	u8								m_tintA;
};

//////////////////////////////////////////////////////////////////////////

class CPacketDecalFragmentDestroy : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDecalFragmentDestroy(u32 tagHashName, Vec3V_In &pos, float scale);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DECAL_FRAGMENTDESTROY, "Validation of CPacketEntityFragDestroyFx2 Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DECAL_FRAGMENTDESTROY;
	}

protected:

	u32								m_tagHashName;
	CPacketVector3					m_pos;
	float							m_scale;
};

#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // INC_ENTITYFXPACKET_H_
