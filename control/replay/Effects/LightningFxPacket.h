#ifndef INC_LIGHTNINGFXPACKET_H_
#define INC_LIGHTNINGFXPACKET_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"
#include "control/replay/Effects/ParticlePacket.h"
#if INCLUDE_FROM_GAME
#include "vfx/systems/VfxLightning.h"
#endif

//////////////////////////////////////////////////////////////////////////
struct PacketLightningBlast
{
	CPacketVector<2>		m_OrbitPos;
	float					m_Intensity;
	float					m_Duration;
	float					m_size;
};

//////////////////////////////////////////////////////////////////////////
class CPacketDirectionalLightningFxPacket : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDirectionalLightningFxPacket(const Vector2& rootOrbitPos, u8 blastCount, LightningBlast* pBlasts, const Vector3& direction, float flashAmount);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DIRECTIONALLIGHTNING, "Validation of CPacketDirectionalLightningFxPacket Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DIRECTIONALLIGHTNING;
	}

private:
	CPacketVector<2>		m_rootOrbitPos;
	u8						m_blastCount;
	PacketLightningBlast	m_lightningBlasts[MAX_LIGHTNING_BLAST];
	CPacketVector<3>		m_flashDirection;
	float					m_flashAmount;
};



//////////////////////////////////////////////////////////////////////////
struct PacketCloudBurst
{
	CPacketVector<2>		m_rootOrbitPos;
	float					m_delay;
	u8						m_blastCount;
	PacketLightningBlast	m_lightningBlasts[MAX_LIGHTNING_BLAST];
};


//////////////////////////////////////////////////////////////////////////
class CPacketCloudLightningFxPacket : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketCloudLightningFxPacket(const Vector2& rootOrbitPos, u8 dominantIndex, int lightCount, LightingBurstSequence* pSequence);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CLOUDLIGHTNING, "Validation of CPacketCloudLightningFxPacket Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_CLOUDLIGHTNING;
	}

private:
	CPacketVector<2>		m_rootOrbitPos;
	u8						m_dominantIndex;

	u8						m_burstCount;
	PacketCloudBurst		m_cloudBursts[MAX_NUM_CLOUD_LIGHTS];
};




//////////////////////////////////////////////////////////////////////////
class CPacketLightningStrikeFxPacket : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketLightningStrikeFxPacket(const Vec3V& flashDir, const CVfxLightningStrike& strike);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_LIGHTNINGSTRIKE, "Validation of CPacketLightningStrikeFxPacket Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_LIGHTNINGSTRIKE;
	}

private:
	CPacketVector<3>		m_flashDirection;

	SLightningSegment		m_segments[NUM_LIGHTNING_SEGMENTS];
	u8						m_numSegments;

	float					m_duration;

	float					m_keyframeLerpMainIntensity;
	float					m_keyframeLerpBranchIntensity;
	float					m_keyframeLerpBurst;
	float					m_intensityFlickerMult;
	Color32					m_colour;
	CPacketVector<2>		m_horizDir;
	float					m_height;
	float					m_distance;
	float					m_animationTime;
	bool					m_isBurstLightning;
};


//////////////////////////////////////////////////////////////////////////
class CPacketRequestCloudHat : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketRequestCloudHat(bool scripted, int index, float transitionTime, bool assumedMaxLength, int prevIndex, float prevTransitionTime, bool prevAssumedMaxLength);

	void Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_REQUESTCLOUDHAT, "Validation of CPacketRequestCloudHat Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_REQUESTCLOUDHAT;
	}

	bool	HasExpired(const CEventInfo<void>&) const;
	u32				GetExpiryTrackerID() const { return m_index; }

private:

	bool m_scripted;
	int m_index;
	float m_transitionTime;
	bool m_assumedMaxLength;

	int m_prevIndex;
	float m_prevTransitionTime;
	bool m_prevAssumedMaxLength;
};

//////////////////////////////////////////////////////////////////////////
class CPacketUnloadAllCloudHats : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketUnloadAllCloudHats();

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_UNLOADALLCLOUDHATS, "Validation of CPacketUnloadAllCloudHats Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_UNLOADALLCLOUDHATS;
	}

private:

};

#endif // GTA_REPLAY

#endif // INC_LIGHTNINGFXPACKET_H_
