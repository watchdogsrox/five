#ifndef VEHICLEAMPHQUADPACKET_H_
#define VEHICLEAMPHQUADPACKET_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"

class CAmphibiousQuadBike;

//////////////////////////////////////////////////////////////////////////
class CPacketAmphQuadUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CAmphibiousQuadBike *pQuadbike, bool storeCreateInfo);
	void		StoreExtensions(CAmphibiousQuadBike *pQuadbike, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketAmphQuadUpdate, CAmphibiousQuadBike>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_AMPHQUADUPDATE, "Validation of CPacketAmphQuadUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketAmphQuadUpdate), "Validation of CPacketAmphQuadUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_AMPHQUADUPDATE;
	}

	DECLARE_PACKET_EXTENSION(CPacketAmphQuadUpdate);

	bool		m_isInWater;
	bool		m_IsInAmphibiousBoatMode;
};


#endif // GTA_REPLAY

#endif // VEHICLEAMPHQUADPACKET_H_