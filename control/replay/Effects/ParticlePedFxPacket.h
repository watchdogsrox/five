//=====================================================================================================
// name:		ParticlePedFxPacket.h
// description:	Ped Fx particle replay packet.
//=====================================================================================================

#ifndef INC_PEDFXPACKET_H_
#define INC_PEDFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Effects\ParticlePacket.h"

class CPed;

//=====================================================================================================
class CPacketPedSmokeFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedSmokeFx(u32 uPfxHash);

	void	Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PEDSMOKEFX, "Validation of CPacketPedSmokeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PEDSMOKEFX;
	}

private:
	u32		m_pfxHash;
};


//=====================================================================================================
class CPacketPedBreathFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedBreathFx(u32 uPfxHash, float fSpeed, float fTemp, float fRate);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PEDBREATHFX, "Validation of CPacketPedBreathFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PEDBREATHFX;
	}

private:
	u32		m_pfxHash;
	float	m_fSpeed;
	float	m_fTemp;
	float	m_fRate;
};


//////////////////////////////////////////////////////////////////////////
class CPacketPedFootFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedFootFx(u32 uPfxHash, float depth, float speed, float scale, u32 foot);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PEDFOOTFX, "Validation of CPacketPedFootFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PEDFOOTFX;
	}

private:
	u32		m_pfxHash;
	float	m_fSize;
	u8		m_bIsMale		:1;
	u8		m_bIsLeft		:1;
	u8		m_Pad			:6;

	float	m_depthEvo;
	float	m_speedEvo;
	float	m_ptFxScale;
	u32		m_foot;
};


//////////////////////////////////////////////////////////////////////////
class CPacketPlayerSwitch : public CPacketEvent, public CPacketEntityInfo<2>
{
public:
	CPacketPlayerSwitch();

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PLAYERSWITCH, "Validation of CPacketPlayerSwitch Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PLAYERSWITCH;
	}
};


#endif // GTA_REPLAY

#endif // INC_PEDFXPACKET_H_
