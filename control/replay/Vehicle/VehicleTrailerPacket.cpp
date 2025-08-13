#include "control/replay/Vehicle/VehicleTrailerPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "Vehicles/Trailer.h"


//////////////////////////////////////////////////////////////////////////
void CPacketTrailerUpdate::Store(CTrailer* pTrailer, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketTrailerUpdate);	
	CPacketVehicleUpdate::Store(PACKETID_TRAILERUPDATE, sizeof(CPacketTrailerUpdate), pTrailer, storeCreateInfo);

	m_parentID = NoEntityID;	// Remove
}


void CPacketTrailerUpdate::StoreExtensions(CTrailer* pTrailer, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketTrailerUpdate);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_TRAILERUPDATE, sizeof(CPacketTrailerUpdate), pTrailer, storeCreateInfo);
}


#endif // GTA_REPLAY