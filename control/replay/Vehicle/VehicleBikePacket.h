//
// name:		VehicleBikePacket.h
// description:	Derived from CVehiclePacket, this class handles all bike-related replay packets.
//				See VehiclePacket.h for a full description of all the methods.
// written by:	Al-Karim Hemraj (R* Toronto)
//

#ifndef VEHBIKEPACKET_H
#define VEHBIKEPACKET_H

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"

class CBike;
class CBmx;
class CQuadBike;

//////////////////////////////////////////////////////////////////////////
class CPacketBikeUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CBike *pBike, bool storeCreateInfo);
	void		StoreExtensions(CBike *pBike, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketBikeUpdate, CBike>& info) const;

	float		GetUVAnimValueY() const { return m_uvAnimValueY; }
	float		GetUVAnimValueX() const { return m_uvAnimValueX; }
	bool		GetAnimFwd() const		{ return m_chainAnimFwd; }

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BIKEUPDATE, "Validation of CPacketBikeUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketBikeUpdate), "Validation of CPacketBikeUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_BIKEUPDATE;
	}

private:
#if !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	s8			m_LeanAngle;
#endif // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	DECLARE_PACKET_EXTENSION(CPacketBikeUpdate);

	float		m_uvAnimValueY;
	float		m_uvAnimValueX;

	bool		m_chainAnimFwd;
};



//////////////////////////////////////////////////////////////////////////
class CPacketBicycleUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CBmx *pBicycle, bool storeCreateInfo);
	void		StoreExtensions(CBmx *pBicycle, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketBicycleUpdate, CBmx>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BICYCLEUPDATE, "Validation of CPacketBicycleUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketBicycleUpdate), "Validation of CPacketBicycleUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_BICYCLEUPDATE;
	}

private:
#if !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	s8			m_LeanAngle;	// See CPacketVehicleUpdate.
	bool		m_IsPedalling;
#endif // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	f32			m_PedallingRate;
	DECLARE_PACKET_EXTENSION(CPacketBicycleUpdate);
};


//////////////////////////////////////////////////////////////////////////
class CPacketQuadBikeUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CQuadBike *pQuadbike, bool storeCreateInfo);
	void		StoreExtensions(CQuadBike *pQuadbike, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketQuadBikeUpdate, CQuadBike>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_QUADBIKEUPDATE, "Validation of CPacketQuadBikeUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketQuadBikeUpdate), "Validation of CPacketQuadBikeUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_QUADBIKEUPDATE;
	}
	DECLARE_PACKET_EXTENSION(CPacketQuadBikeUpdate);
};

#	endif // GTA_REPLAY

#endif  // VEHBIKEPACKET_H
