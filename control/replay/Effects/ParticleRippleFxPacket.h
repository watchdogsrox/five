//=====================================================================================================
// name:		ParticleRippleFxPacket.h
// description:	Ripple Fx particle replay packet.
//=====================================================================================================

#ifndef INC_RIPPLEFXPACKET_H_
#define INC_RIPPLEFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Effects\ParticlePacket.h"
#include "control\replay\Misc\ReplayPacket.h"
#include "vector\vector3.h"

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

class CEntity;

//=====================================================================================================
class CPacketRippleWakeFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:

	CPacketRippleWakeFx(const Vector3& rPos, bool bFlipTexture, s32 eWakeType, u16 uBoneIdx = 0);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_RIPPLEWAKEFX, "Validation of CPacketRippleWakeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_RIPPLEWAKEFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StorePos(const Vector3& rVecPos);
	void LoadPos(Vector3& oVecPos) const;

	//-------------------------------------------------------------------------------------------------
	float	m_Position[3];
	u8		m_bFlipTexture	:1;
	u8		m_eWakeType		:2;
	u8		m_uPadBits		:5;
	u16		m_uBoneIdx;
	u8		m_Pad;
};

//=====================================================================================================
class CPacketDynamicWaveFx : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDynamicWaveFx(s32 x, s32 y, float fSpeedDown, float fCustom);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool ShouldInvalidate() const		{	return false;	}
	void Invalidate()	{ 	}

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DYNAMICWAVEFX, "Validation of CPacketDynamicWaveFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DYNAMICWAVEFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StorePosInt(const s32& x, const s32& y, const float& z);
	void LoadPosInt(s32& x, s32& y, float& z) const;
	//-------------------------------------------------------------------------------------------------
	union
	{
		struct
		{
			float	m_fPosition[3];
		};
		s32	m_sPosition[3];
	};

	float	m_fCustom;
};

#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // INC_RIPPLEFXPACKET_H_
