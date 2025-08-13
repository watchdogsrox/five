//=====================================================================================================
// name:		FootStepPacket.h
// description:	FootStep replay packet.
//=====================================================================================================

#ifndef INC_FOOTSTEPPACKET_H_
#define INC_FOOTSTEPPACKET_H_

#include "Control/Replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "Control/Replay/Effects/ParticlePacket.h"
#include "Control/Replay/PacketBasics.h"

class CPed;

class CKeyValuePair
{
public:
	CKeyValuePair(){}
	CKeyValuePair(u32 k, float v)
		: key(k)
		, value(v)
	{}
	 
	u32	key;
	float value;
};

//////////////////////////////////////////////////////////////////////////
class CPacketFootStepSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketFootStepSound(u8 event);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SOUND_PEDFOOTSTEP, "Validation of CPacketFootStepSound Failed!, %d", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SOUND_PEDFOOTSTEP;
	}

private:
	u8		m_event;
	//u8		m_unused[3];
};


//////////////////////////////////////////////////////////////////////////
class CPacketPedBushSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedBushSound(u8 event);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:
	u8		m_event;
	//u8		m_unused[3];
};


//////////////////////////////////////////////////////////////////////////
class CPacketPedClothSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedClothSound(u8 event);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:
	u8		m_event;
	//u8		m_unused[3];
};


//////////////////////////////////////////////////////////////////////////
class CPacketPedPetrolCanSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedPetrolCanSound(u8 event);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:
	u8		m_event;
	//u8		m_unused[3];
};

//////////////////////////////////////////////////////////////////////////
class CPacketPedMolotovSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedMolotovSound(u8 event);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:
	u8		m_event;
	//u8		m_unused[3];
};

//////////////////////////////////////////////////////////////////////////
class CPacketPedScriptSweetenerSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedScriptSweetenerSound(u8 event);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:
	u8		m_event;
	//u8		m_unused[3];
};

//////////////////////////////////////////////////////////////////////////
class CPacketPedWaterSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedWaterSound(u8 event);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:
	u8		m_event;
	//u8		m_unused[3];
};


//////////////////////////////////////////////////////////////////////////
class CPacketPedSlopeDebrisSound : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedSlopeDebrisSound(u8 event, const Vec3V& slopeDir, float slopeAngle);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:

	u8		m_event;
	//u8		m_unused[3];
	CPacketVector3		m_slopeDirection;
	float				m_slopeAngle;
};



//////////////////////////////////////////////////////////////////////////
class CPacketPedStandingMaterial : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedStandingMaterial(u32 materialNameHash);

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const;
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}

private:
	u32					m_materialNameHash;
};


/////////////////////////////////////////////////////////////////////////
class CPacketFallToDeath : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketFallToDeath();

	void			Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void			PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool			ValidatePacket() const				{	return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket();		}
};

#endif // GTA_REPLAY

#endif // INC_PFXPACKET_H_
