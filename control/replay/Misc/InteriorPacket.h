#ifndef __INTERIOR_PACKET_H__
#define __INTERIOR_PACKET_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/PacketBasics.h"
#include "control/replay/ReplayEnums.h"
#include "control/replay/Misc/InteriorPacket.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/Effects/ParticlePacket.h"


#include "scene/portals/InteriorProxy.h"


#define MAX_REPLAY_INTERIOR_SAVES 300

class CPacketInterior : public CPacketBase
{
public:
	CPacketInterior()	: CPacketBase(PACKETID_INTERIOR, sizeof(CPacketInterior))	{}

	void Store(const atArray<ReplayInteriorData>& info);
	void Extract() const  {}
	void Setup() const;

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_INTERIOR, "Validation of CPacketInterior Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_INTERIOR;
	}

	int		GetCount() const		{ return m_count; }
	const ReplayInteriorData* GetData() const { return m_InteriorData; }

private:

	int m_count;
	ReplayInteriorData	m_InteriorData[MAX_REPLAY_INTERIOR_SAVES];
};


/////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_ENTITY_SET_UPDATES 512
#define MAX_ENTITY_SETS_OLD 16
// Interior Entity Set Packet
class CPacketInteriorEntitySet : public CPacketBase
{
public:
	CPacketInteriorEntitySet()	: CPacketBase(PACKETID_INTERIOR_ENTITY_SET, sizeof(CPacketInteriorEntitySet))	{}

	void Store(const u32 nameHash, Vec3V &position, const atArray<atHashString>& info);
	void Extract(bool isJumping) const;
private:
	int FindDeferredEntry(u32 interiorProxyID) const;

public:
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_INTERIOR_ENTITY_SET, "Validation of CPacketInteriorEntitySet Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_INTERIOR_ENTITY_SET;
	}

public:
	static void ProcessDeferredEntitySetUpdates();

private:
	static bool AreEntitySetsTheSame(CInteriorProxy *pIntProxy, atFixedArray <u32, MAX_ENTITY_SETS_OLD> const &entitySetHashValues);

	union
	{
		u32		m_InteriorProxyID;			// Initial Version
		u32		m_InteriorProxyNameHash;	// Version 1
	};
	atFixedArray<u32, MAX_ENTITY_SETS_OLD> m_activeEntitySets;

	// Version 1 variables added
	CPacketVector<3> m_InteriorProxyPos;
};

struct PackedEntitySet
{
	u32 m_name;
	int m_limit;
	int m_tintIdx;

	void operator=(const SEntitySet& rhs) { m_name=rhs.m_name; m_limit=rhs.m_limit; m_tintIdx=rhs.m_tintIdx; }
	bool operator!=(const SEntitySet& rhs){ return m_name!=rhs.m_name || m_limit!=rhs.m_limit || m_tintIdx!=rhs.m_tintIdx; }
	operator SEntitySet() const { return SEntitySet(m_name,m_limit,m_tintIdx); }
};

// Interior Entity Set Packet....that doesn't rely on a game defined size somewhere
class CPacketInteriorEntitySet2 : public CPacketBase
{
public:

	void Store(const u32 nameHash, Vec3V &position, const atArray<SEntitySet>& info);
	void StoreExtensions(const u32 /*nameHash*/, Vec3V &/*position*/, const atArray<SEntitySet>& /*info*/){}
	void Extract(bool isJumping) const;
private:
	int FindDeferredEntry(u32 interiorProxyID) const;

public:
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_INTERIOR_ENTITY_SET2, "Validation of CPacketInteriorEntitySet2 Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_INTERIOR_ENTITY_SET2;
	}

public:
	static void ProcessDeferredEntitySetUpdates();

private:

	CPacketInteriorEntitySet2()	: CPacketBase(PACKETID_INTERIOR_ENTITY_SET2, sizeof(CPacketInteriorEntitySet2))	{}

	bool		AreEntitySetsTheSame(CInteriorProxy *pIntProxy) const;

	u32*		GetActiveEntitySetPtr() const	{ return (u32*)((u8*)this + GetPacketSize() - (m_numActiveEntitySets * sizeof(u32))); }

	PackedEntitySet*	GetActiveEntitySetPtrAsPackedEntitySet() const	{ return (PackedEntitySet*)((u8*)this + GetPacketSize() - (m_numActiveEntitySets * sizeof(PackedEntitySet))); }

	union
	{
		u32		m_InteriorProxyID;			// Initial Version
		u32		m_InteriorProxyNameHash;	// Version 1
	};

	CPacketVector<3>	m_InteriorProxyPos;
	u32					m_numActiveEntitySets;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////

struct CRoomRequest
{
	u32 m_ProxyNameHash;
	u32 m_positionHash;
	u32 m_RoomIndex;
};


// Interior Proxy state packet, recorded by the game interface.
class CPacketInteriorProxyStates : public CPacketBase
{
public:
	CPacketInteriorProxyStates() : CPacketBase(PACKETID_INTERIOR_PROXY_STATE, sizeof(CPacketInteriorProxyStates))	{}
public:
	void Store(const atArray <CRoomRequest> &roomRequests);
	void StoreExtensions(const atArray <CRoomRequest> &roomRequests) { PACKET_EXTENSION_RESET(CPacketInteriorProxyStates); (void)roomRequests; }
	void Extract() const {};
	bool AddPreloadRequests(class ReplayInteriorManager &interiorManager, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason)) const;

	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_INTERIOR_PROXY_STATE, "Validation of CPacketInteriorProxyStates Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_INTERIOR_PROXY_STATE;
	}

private:
	u32 *GetRoomRequests() const;
	u32 *GetInteriorProxyIDs() const;

	u32 m_NoOfInteriorProxies;
	u32 m_NoOfRoomRequests;
	DECLARE_PACKET_EXTENSION(CPacketInteriorProxyStates);
};


class CPacketForceRoomForGameViewport : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketForceRoomForGameViewport(CInteriorProxy* pInteriorProxy, int roomKey);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }
	bool HasExpired(const CEventInfo<void>&) const;

	//Need to always extract this for every block to be able to support rewinding.
	eShouldExtract	ShouldExtract(u32, const u32, bool) const { return REPLAY_EXTRACT_SUCCESS; } 
	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FORCE_ROOM_FOR_GAMEVIEWPORT, "Validation of CPacketForceRoomForGameViewport Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FORCE_ROOM_FOR_GAMEVIEWPORT;
	}

	static int sm_ExpirePrevious;
private:

	CPacketVector<3> m_InteriorProxyPos;
	u32	m_InteriorProxyNameHash;	
	int m_RoomKey;
};


//////////////////////////////////////////////////////////////////////////

class CPacketForceRoomForEntity_OLD : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketForceRoomForEntity_OLD(CInteriorProxy* pInteriorProxy, int roomKey);

	void			Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FORCE_ROOM_FOR_ENTITY_OLD, "Validation of CPacketForceRoomForEntity Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FORCE_ROOM_FOR_ENTITY_OLD;
	}

	bool				HasExpired(const CEventInfo<CEntity>& eventInfo) const;
	bool				NeedsEntitiesForExpiryCheck() const { return true; }

protected:

	CPacketVector<3> m_InteriorProxyPos;
	u32	m_InteriorProxyNameHash;	
	int m_RoomKey;
};

//////////////////////////////////////////////////////////////////////////

class CPacketForceRoomForEntity : public CPacketEventTracked, public CPacketEntityInfoStaticAware<1>
{
public:
	CPacketForceRoomForEntity(CInteriorProxy* pInteriorProxy, int roomKey);

	void			Extract(CTrackedEventInfo<int>& eventInfo) const;
	ePreloadResult	Preload(const CTrackedEventInfo<int>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<int>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FORCE_ROOM_FOR_ENTITY, "Validation of CPacketForceRoomForEntity Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FORCE_ROOM_FOR_ENTITY;
	}

	bool		ShouldEndTracking() const						{	return false;				}
	bool		ShouldStartTracking() const						{	return true;				}

	bool	Setup(bool)
	{
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<int>&) const;
	bool				NeedsEntitiesForExpiryCheck() const { return true; }

protected:

	CPacketVector<3> m_InteriorProxyPos;
	u32	m_InteriorProxyNameHash;	
	int m_RoomKey;
};

//////////////////////////////////////////////////////////////////////////



class CPacketModelCull : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketModelCull(u32 modelHash);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MODEL_CULL, "Validation of CPacketModelCull Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_MODEL_CULL;
	}
private:

	u32	m_ModelHash;	
};


class CPacketDisableOcclusion : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDisableOcclusion();

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DISABLE_OCCLUSION, "Validation of CPacketDisableOcclusion Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DISABLE_OCCLUSION;
	}
};

#endif	//GTA_REPLAY

#endif	//__INTERIOR_PACKET_H__
