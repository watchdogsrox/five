#ifndef __RAY_FIRE_COMMAND_PACKET_H__
#define __RAY_FIRE_COMMAND_PACKET_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/Effects/ParticlePacket.h"
#include "Control/Replay/PacketBasics.h"
#include "modelInfo/CompEntityModelInfo.h"

//-------------------------------------------------------------------------------

class PacketRayFireInfo
{
public:

	enum
	{
		STATE_START = 0,
		STATE_END,
		STATE_ANIMATING
	};

	u32					m_ModelHash;
	CPacketVector <3>	m_ModelPos;
	u32					m_State;
	float				m_Phase;			// The phase this frame
};

class CPacketRayFireStatic : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketRayFireStatic(u32 modelHash, Vector3 &pos, u32 state, float phase);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_RAYFIRE_STATIC, "Validation of CPacketRayFireStatic Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_RAYFIRE_STATIC;
	}

private:

	PacketRayFireInfo	m_Info;
};


class CPacketRayFireUpdating : public CPacketEventInterp, public CPacketEntityInfo<0>
{
public:
	CPacketRayFireUpdating(u32 modelHash, Vector3 &pos, u32 state, float phase, bool stopTracking);

	void Extract(const CInterpEventInfo<CPacketRayFireUpdating, void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool		ShouldStartTracking() const						{	return m_stopTracking == false;		}
	bool		ShouldEndTracking() const						{	return m_stopTracking == true;		}
	bool		ShouldTrack() const								{	return true;				}

	void PrintXML(FileHandle handle) const	{	CPacketEventInterp::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);	}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_RAYFIRE_UPDATING, "Validation of CPacketRayFireUpdating Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_RAYFIRE_UPDATING;
	}

private:

	PacketRayFireInfo	m_Info;
	bool				m_stopTracking;
};


//-------------------------------------------------------------------------------

// Data needed for pre-load of a rayfires assets. (see void CCompEntity::PreLoadAllAssets())

class PacketRayFirePreLoadData
{
public:

	bool					m_Load;						// If this is the start or end of a rayfires data requirements

	// Stuff that comes from the compentity model info that is needed to load some data.
	u32						m_ModelHash;				// For ID-ing this data in the compentity code, the rayfire model hash.

	u32						m_UniqueID;					// For a unique ID for this compentity? Hash of position? Ask Ian if there's something better.

	bool					m_bUseImapForStartAndEnd;	// If the start/end states are imaps or models.
	u32						m_startIMapOrModelHash;		// The hash for the Start imaps/models
	u32						m_endIMapOrModelHash;		// The hash for the End imaps/models
	u32						m_ptfxAssetNameHash;		// The hash	for the particle data
	atFixedArray<u32, 10>	m_animModelHashes;			// Comes in pairs with models/anims for the anim stage of rayfire.
	atFixedArray<u32, 10>	m_animHashes;
};


// A packet to tell the system when to pre-load some rayfire data
class CPacketRayFirePreLoad : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketRayFirePreLoad(bool loading, u32 uid, CCompEntityModelInfo *pCompEntityModelInfo);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&eventInfo) const;
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_RAYFIRE_PRE_LOAD, "Validation of CPacketRayFirePreLoad Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_RAYFIRE_PRE_LOAD;
	}

	PacketRayFirePreLoadData &GetInfo()	{ return m_Info; }

private:

	PacketRayFirePreLoadData	m_Info;
};

//-------------------------------------------------------------------------------

#endif	//GTA_REPLAY

#endif	//__RAY_FIRE_COMMAND_PACKET_H__