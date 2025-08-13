#ifndef VEHICLETRAILERPACKET_H
#define VEHICLETRAILERPACKET_H

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"

class CTrailer;

//////////////////////////////////////////////////////////////////////////
class CPacketTrailerUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CTrailer* pTrailer, bool storeCreateInfo);
	void		StoreExtensions(CTrailer* pTrailer, bool storeCreateInfo);

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRAILERUPDATE, "Validation of CPacketTrailerUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketTrailerUpdate), "Validation of CPacketTrailerUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_TRAILERUPDATE;
	}

	void		PrintXML(FileHandle handle) const
	{
		CPacketVehicleUpdate::PrintXML(handle);

		char str[1024];
		snprintf(str, 1024, "\t\t<ParentID>0x%X, %d</ParentID>\n", m_parentID.ToInt(), m_parentID.ToInt());
		CFileMgr::Write(handle, str, istrlen(str));
	}

private:
	CReplayID	m_parentID;	// Remove
	DECLARE_PACKET_EXTENSION(CPacketTrailerUpdate);
};


#endif // GTA_REPLAY

#endif // VEHICLETRAILERPACKET_H
