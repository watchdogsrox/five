#ifndef VEHBOATPACKET_H
#define VEHBOATPACKET_H

#include "Control/replay/ReplaySettings.h"

#	if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"

class CBoat;

//////////////////////////////////////////////////////////////////////////
class CPacketBoatUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CBoat *pBoat, bool storeCreateInfo);
	void		StoreExtensions(CBoat *pBoat, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketBoatUpdate, CBoat>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BOATUPDATE, "Validation of CPacketBoatUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketBoatUpdate), "Validation of CPacketBoatUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_BOATUPDATE;
	}

protected:
	float		m_rudderAngle;

	Vector3		m_propDeformationOffset1;
	Vector3		m_propDeformationOffset2;

	bool		m_isInWater : 1;
	bool		m_ReplayVehiclePhysicsInWater : 1;
	DECLARE_PACKET_EXTENSION(CPacketBoatUpdate);
};

#	endif // GTA_REPLAY

#endif  // VEHBOATPACKET_H
