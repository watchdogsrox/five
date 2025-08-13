//=====================================================================================================
// name:		PickupPacket.h
// description:	Pickup replay packet.
//=====================================================================================================

#ifndef INC_PICKUPPACKET_H_
#define INC_PICKUPPACKET_H_

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/ReplaySupportClasses.h"
#include "control/replay/Misc/InterpPacket.h"
#include "Control/Replay/PacketBasics.h"
#include "streaming/streamingdefs.h"

class CObject;
class CPickup;
namespace rage
{ class Vector3; }

//////////////////////////////////////////////////////////////////////////
// Packet for creation of an Pickup
const tPacketVersion CPacketPickupCreate_V1 = 1;
class CPacketPickupCreate : public CPacketBase, public CPacketEntityInfo<1>
{
public:
	CPacketPickupCreate();

	void	Store(const CPickup* pObject, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId = PACKETID_PICKUP_CREATE, tPacketSize packetSize = sizeof(CPacketPickupCreate));
	void	StoreExtensions(const CPickup* pObject, bool firstCreationPacket, u16 sessionBlockIndex);
	void	CloneExtensionData(const CPacketPickupCreate* pSrc) { if(pSrc) { CLONE_PACKET_EXTENSION_DATA(pSrc, this, CPacketPickupCreate); } }

	void	SetFrameCreated(const FrameRef& frame)	{	m_FrameCreated = frame;	}
	FrameRef GetFrameCreated() const				{	return m_FrameCreated;	}
	bool	IsFirstCreationPacket() const			{	return m_firstCreationPacket;	}

	void	Reset()						{ SetReplayID(-1); m_FrameCreated = FrameRef::INVALID_FRAME_REF; }

	u32					GetModelNameHash() const	{	return m_ModelNameHash;	}
	strLocalIndex		GetMapTypeDefIndex() const	{	return (strLocalIndex)m_MapTypeDefIndex; }
	void				SetMapTypeDefIndex(strLocalIndex index) { m_MapTypeDefIndex = index.Get();}
	u32					GetMapTypeDefHash() const { return m_MapTypeDefHash; }
	bool	UseMapTypeDefHash() const { return GetPacketVersion() >= CPacketPickupCreate_V1; }
	u32					GetPickupHash() const		{	return m_PickupHash; }
	CReplayID			GetParentID() const;
	u32					GetWeaponHash() const;
	u8					GetWeaponTint() const;
	bool				IsWeapon() const			{	return GetWeaponHash() != 0;	}

	bool	ValidatePacket() const
	{
		replayAssertf(((GetPacketID() == PACKETID_PICKUP_CREATE) || (GetPacketID() == PACKETID_PICKUP_DELETE)), "Validation of CPacketPickupCreate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketPickupCreate), "Validation of CPacketPickupCreate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketPickupCreate) && ((GetPacketID() == PACKETID_PICKUP_CREATE) || (GetPacketID() == PACKETID_PICKUP_DELETE));
	}

	void				PrintXML(FileHandle handle) const;
	
	struct PickupCreateExtension
	{
		// CPacketPickupCreateExtensions_V1
		CReplayID	m_parentID;
		
		// CPacketPickupCreateExtensions_V2
		u32			m_weaponHash;

		// CPacketPickupCreateExtensions_V3
		u8			m_weaponTint;
		u8			m_unused[3];
	};

private:
	FrameRef			m_FrameCreated;
	bool				m_firstCreationPacket;

	u32					m_ModelNameHash;
	union
	{
		s32			m_MapTypeDefIndex; //note: only 12 bit of this is used
		u32			m_MapTypeDefHash;
	};
	u32					m_PickupHash;
	DECLARE_PACKET_EXTENSION(CPacketPickupCreate);
};


//////////////////////////////////////////////////////////////////////////
class CPacketPickupUpdate : public CPacketBase, public CPacketInterp, public CBasicEntityPacketData
{
public:

	void	Store(CPickup* pObject, bool firstUpdatePacket);
	void	StoreExtensions(CPickup* pObject, bool firstUpdatePacket) { PACKET_EXTENSION_RESET(CPacketPickupUpdate); (void)pObject; (void)firstUpdatePacket; }
	void	Store(eReplayPacketId uPacketID, u32 packetSize, CPickup* pObject, bool firstUpdatePacket);
	void	PreUpdate(const CInterpEventInfo<CPacketPickupUpdate, CPickup>& eventInfo) const;
	void	Extract(const CInterpEventInfo<CPacketPickupUpdate, CPickup>& eventInfo) const;

	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_PICKUP_UPDATE, "Validation of CPacketPickupUpdate Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketInterp::ValidatePacket() && CBasicEntityPacketData::ValidatePacket() && GetPacketID() == PACKETID_PICKUP_UPDATE;
	}

	void				PrintXML(FileHandle handle) const;

	bool	m_firstUpdatePacket;
	DECLARE_PACKET_EXTENSION(CPacketPickupUpdate);

	float	m_scaleXY;
	float	m_scaleZ;
};


//////////////////////////////////////////////////////////////////////////
// Packet for deletion of an Object
class CPacketPickupDelete : public CPacketPickupCreate
{
public:
	void	Store(const CPickup* pObject, const CPacketPickupCreate* pLastCreatedPacket);
	void	StoreExtensions(const CPickup*, const CPacketPickupCreate* pLastCreatedPacket) { CPacketPickupCreate::CloneExtensionData(pLastCreatedPacket);  PACKET_EXTENSION_RESET(CPacketPickupDelete); }
	void	Cancel()					{	SetReplayID(-1);	}

	void	PrintXML(FileHandle handle) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PICKUP_DELETE, "Validation of CPacketPickupDelete Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketPickupDelete), "Validation of CPacketPickupDelete extensions Failed!, 0x%08X", GetPacketID());
		return CPacketPickupCreate::ValidatePacket() && VALIDATE_PACKET_EXTENSION(CPacketPickupDelete) && GetPacketID() == PACKETID_PICKUP_DELETE;
	}

	const CPacketPickupCreate*	GetCreatePacket() const	{	return this;	}

private:
	DECLARE_PACKET_EXTENSION(CPacketPickupDelete);
};

#define MAX_PICKUP_DELETION_SIZE	sizeof(CPacketPickupDelete) + sizeof(CPacketPickupCreate::PickupCreateExtension) + sizeof(CPacketExtension::PACKET_EXTENSION_GUARD)

//////////////////////////////////////////////////////////////////////////
class CPickupInterp : public CInterpolator<CPacketPickupUpdate>
{
public:
	CPickupInterp() { Reset(); }
	virtual ~CPickupInterp() {}

	void Init(const ReplayController& controller, CPacketPickupUpdate const* pPrevPacket);

	void Reset();

	s32	GetPrevFullPacketSize() const	{ return m_prevData.m_fullPacketSize; }
	s32	GetNextFullPacketSize() const	{ return m_nextData.m_fullPacketSize; }

	struct CPickupInterpData  
	{
		void Reset()
		{
			memset(this, 0, sizeof(CPickupInterpData));
		}
		s32							m_fullPacketSize;
	} m_prevData, m_nextData;

};


#endif // GTA_REPLAY

#endif // INC_PICKUPPACKET_H_
