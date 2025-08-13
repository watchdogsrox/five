//=====================================================================================================
// name:		ParticleMaterialFxPacket.h
// description:	Material Fx particle replay packet.
//=====================================================================================================

#ifndef INC_MATERIALFXPACKET_H_
#define INC_MATERIALFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Effects\ParticlePacket.h"

class CEntity;
namespace rage
{class Matrix34;}

//////////////////////////////////////////////////////////////////////////
class CPacketMaterialBangFx : public CPacketEvent, public CPacketEntityInfoStaticAware<1>
{
public:
	CPacketMaterialBangFx(u32 uPfxHash, const rage::Matrix34& rMatrix, float velocityEvo, float impulseEvo, float lodRangeScale);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MATERIALBANGFX, "Validation of CPacketMaterialBangFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_MATERIALBANGFX;
	}

private:

	CPacketPositionAndQuaternion	m_posAndRot;

	u32		m_pfxHash;

	float	m_velocityEvo;
	float	m_impulseEvo;
	float	m_bangScrapePtFxLodRangeScale;
};

#endif // GTA_REPLAY

#endif // INC_MATERIALFXPACKET_H_
