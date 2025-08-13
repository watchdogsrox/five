//
// name:		VehiclePlanePacket.h
// description:	This class handles all plane-related replay packets. Planes are Entities, simple position storing.
// written by:	Al-Karim Hemraj (R* Toronto)
//

#ifndef VEHPLANEPACKET_H
#define VEHPLANEPACKET_H

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"

class CPlane;

//=====================================================================================================
class CPacketPlaneDelete : public CPacketBase
{
public:
	//-------------------------------------------------------------------------------------------------
	CPacketPlaneDelete();

	void Init(s32 replayID);

	s32	 GetReplayID() const					{ return m_ReplayID; }

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PLANEDELETE, "Validation of CPacketPlaneDelete Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_PLANEDELETE;
	}

protected:
	s32	m_ReplayID;
};


//////////////////////////////////////////////////////////////////////////
class CPacketPlaneUpdate : public CPacketVehicleUpdate
{
public:
	void	Store(CPlane* pPlane, bool storeCreateInfo);
	void	StoreExtensions(CPlane* pPlane, bool storeCreateInfo);
	void	Extract(const CInterpEventInfo<CPacketPlaneUpdate, CPlane>& info) const;
	void	ExtractExtensions(CPlane* pPlane) const;

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PLANEUPDATE, "Validation of CPacketPlaneUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketPlaneUpdate), "Validation of CPacketPlaneUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_PLANEUPDATE;
	}

	float GetSectionHealth(u32 i) const
	{
		if(i==0)
			return m_SectionHealth0;
		else
			return m_SectionHealth1;
	}

	void SetSectionHealth(u32 i, float val) 
	{
		if(i==0)
			m_SectionHealth0 = val;
		else
			m_SectionHealth1 = val;
	}

private:
#if !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	float m_SectionHealth0; // See CPacketVehicleUpdate
#endif // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	float m_SectionHealth1;
	float m_EngineSpeed;

	DECLARE_PACKET_EXTENSION(CPacketPlaneUpdate);

	struct PacketExtensions
	{
		//---------------------- Extensions V1 ------------------------/
		u8	m_LandingGearState;
		u8	m_unused[3];

		//---------------------- Extensions V2 ------------------------/
		float m_verticalFlightRatio;
	};
};


//=====================================================================================================
class CPacketCarriageDelete : CPacketBase
{
public:
	//-------------------------------------------------------------------------------------------------
	CPacketCarriageDelete();

	void Init(s32 replayID);

	s32	 GetReplayID() const					{ return m_ReplayID; }

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CARRIAGEDELETE, "Validation of CPacketCarriageDelete Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_CARRIAGEDELETE;
	}

protected:
	s32	m_ReplayID;
};

//=====================================================================================================
class CPacketCarriageUpdate : public CPacketBase, public CPacketInterp
{
public:
	//-------------------------------------------------------------------------------------------------
	CPacketCarriageUpdate();
	~CPacketCarriageUpdate();

	const CReplayID&	GetReplayID() const		{ return m_ReplayID; }
	u16		GetModelIndex() const	{ return m_ModelIndex; }
	u8		GetCarriageSlot() const { return m_CarriageSlot; }

	void	SetAlphaFade(u8 fade)	{ m_AlphaFade = fade; }
	u8		GetAlphaFade()			{ return m_AlphaFade; }

	void	Store(CPhysical* pCarriage, u8 uSlotIndex);
	void	StoreExtenions(CPhysical* pCarriage, u8 uSlotIndex) { PACKET_EXTENSION_RESET(CPacketCarriageUpdate); (void)pCarriage; (void)uSlotIndex; }
	void	Extract(CPhysical* pCarriage, float fInterp);

	void	Load(Matrix34& rMatrix, Quaternion& rQuatNew, Vector3& rVecNew, float fInterp);
	void	FetchQuaternionAndPos(Quaternion& rQuat, Vector3& rPos);

#if REPLAY_VIEWER
	inline void FillTemplate(CTemplatePacket& roTemplatePacket)
	{ 
		roTemplatePacket.SetPacketID(m_PacketID);
		roTemplatePacket.SetReplayID(m_ReplayID);
		roTemplatePacket.SetModelIndex(m_ModelIndex);
		roTemplatePacket.SetPositionX(m_Position[0]);
		roTemplatePacket.SetPositionY(m_Position[1]);
		roTemplatePacket.SetPositionZ(m_Position[2]);
	}
#endif

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CARRIAGEUPDATE, "Validation of CPacketCarriageUpdate Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketInterp::ValidatePacket() && GetPacketID() == PACKETID_CARRIAGEUPDATE;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void Store(const Matrix34& rMatrix);
	void Load(Matrix34& rMatrix);

	//-------------------------------------------------------------------------------------------------
	u8		m_AlphaFade;
	u8		m_CarriageSlot;
	u16		m_ModelIndex;

	CReplayID	m_ReplayID;

	u32		m_CarriageQuaternion;
	float	m_Position[3];
	DECLARE_PACKET_EXTENSION(CPacketCarriageUpdate);
};

#	endif // GTA_REPLAY

#endif  // VEHPLANEPACKET_H
