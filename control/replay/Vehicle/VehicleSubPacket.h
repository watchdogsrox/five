#ifndef VEHICLESUBPACKET_H
#define VEHICLESUBPACKET_H

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"

class CSubmarine;
class CSubmarineCar;

//////////////////////////////////////////////////////////////////////////
class CPacketSubUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CSubmarine* pSub, bool storeCreateInfo);
	void		StoreExtensions(CSubmarine* pSub, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketSubUpdate, CSubmarine>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SUBUPDATE, "Validation of CPacketSubUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketSubUpdate), "Validation of CPacketSubUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_SUBUPDATE;
	}

private:
	float		m_subPropellorSpeeds[SUB_NUM_PROPELLERS];

	float		m_rudderAngle;
	float		m_elevatorAngle;
	float		m_SubAngularVelocityMag;
	bool		m_ReplayVehiclePhysicsInWater;
	DECLARE_PACKET_EXTENSION(CPacketSubUpdate);
};


//////////////////////////////////////////////////////////////////////////
class CPacketSubCarUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CSubmarineCar* pSub, bool storeCreateInfo);
	void		StoreExtensions(CSubmarineCar* pSub, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketSubCarUpdate, CSubmarineCar>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SUBCARUPDATE, "Validation of CPacketSubCarUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketSubCarUpdate), "Validation of CPacketSubCarUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_SUBCARUPDATE;
	}

private:
	float		m_subPropellorSpeeds[SUB_NUM_PROPELLERS];

	float		m_rudderAngle;
	float		m_elevatorAngle;
	float		m_SubAngularVelocityMag;
	bool		m_ReplayVehiclePhysicsInWater;
	DECLARE_PACKET_EXTENSION(CPacketSubCarUpdate);
};


#endif // GTA_REPLAY

#endif // VEHICLESUBPACKET_H
