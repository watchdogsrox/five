#ifndef VEHICLEAPHAUTOPACKET_H_
#define VEHICLEAPHAUTOPACKET_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY


#include "control/replay/Vehicle/VehiclePacket.h"

class CAmphibiousAutomobile;

//=====================================================================================================
class CPacketAmphAutoUpdate : public CPacketVehicleUpdate
{
public:
	void	Store(CAmphibiousAutomobile *pCar, bool storeCreateInfo);
	void	StoreExtensions(CAmphibiousAutomobile *pCar, bool storeCreateInfo);
	void	Extract(const CInterpEventInfo<CPacketAmphAutoUpdate, CAmphibiousAutomobile>& info) const;

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_AMPHAUTOUPDATE, "Validation of CPacketAmphAutoUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketAmphAutoUpdate), "Validation of CPacketAmphAutoUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_AMPHAUTOUPDATE;
	}

private:
#if !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	u8 m_bOnRevLimiter : 1;
	// Version 3 variables added.
	DECLARE_PACKET_EXTENSION(CPacketAmphAutoUpdate);
#else // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	// Version 3 variables added.
	DECLARE_PACKET_EXTENSION(CPacketAmphAutoUpdate);
	u32 m_unused;
#endif // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64

	bool		m_isInWater;
	bool		m_IsInAmphibiousBoatMode;
};



#endif // GTA_REPLAY

#endif // VEHICLEAPHAUTOPACKET_H_