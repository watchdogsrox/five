//
// name:		VehicleAutomobilePacket.h
// description:	Derived from CVehiclePacket, this class handles all automobile-related replay packets.
//				See VehiclePacket.h for a full description of all the methods.
// written by:	Al-Karim Hemraj (R* Toronto)
//

#ifndef VEHAUTOPACKET_H
#define VEHAUTOPACKET_H

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY


#include "control/replay/Vehicle/VehiclePacket.h"

class CAutomobile;

//=====================================================================================================
class CPacketCarUpdate : public CPacketVehicleUpdate
{
public:
	void	Store(CAutomobile *pCar, bool storeCreateInfo);
	void	StoreExtensions(CAutomobile *pCar, bool storeCreateInfo);
	void	Extract(const CInterpEventInfo<CPacketCarUpdate, CAutomobile>& info) const;

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CARUPDATE, "Validation of CPacketCarUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketCarUpdate), "Validation of CPacketCarUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_CARUPDATE;
	}

private:
#if !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	u8 m_bOnRevLimiter : 1;
	// Version 3 variables added.
	DECLARE_PACKET_EXTENSION(CPacketCarUpdate);
#else // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	// Version 3 variables added.
	DECLARE_PACKET_EXTENSION(CPacketCarUpdate);
	u32 m_unused;
#endif // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
};

#endif  // VEHAUTOPACKET_H
#endif // GTA_REPLAY