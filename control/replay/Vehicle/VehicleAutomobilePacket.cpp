//
// name:		VehicleAutomobilePacket.cpp
//

#include "VehicleAutomobilePacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "game/modelindices.h"
//#include "maths/angle.h"
#include "vehicles/Automobile.h"

//========================================================================================================================
void CPacketCarUpdate::Store(CAutomobile *pCar, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketCarUpdate);
	replayAssert(pCar->GetVehicleType() == VEHICLE_TYPE_CAR || pCar->GetVehicleType() == VEHICLE_TYPE_BOAT);
	
	m_VehiclePacketUpdateSize = sizeof(CPacketCarUpdate);

	CPacketVehicleUpdate::Store(PACKETID_CARUPDATE, sizeof(CPacketCarUpdate), pCar, storeCreateInfo);

	if(pCar && pCar->GetVehicleAudioEntity())
	{
		m_bOnRevLimiter	= pCar->GetVehicleAudioEntity()->GetReplayRevLimiter();
	}	
}


void CPacketCarUpdate::StoreExtensions(CAutomobile *pCar, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketCarUpdate);
	replayAssert(pCar->GetVehicleType() == VEHICLE_TYPE_CAR || pCar->GetVehicleType() == VEHICLE_TYPE_BOAT);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_CARUPDATE, sizeof(CPacketCarUpdate), pCar, storeCreateInfo);
}


void CPacketCarUpdate::Extract(const CInterpEventInfo<CPacketCarUpdate, CAutomobile>& info) const
{
	CAutomobile* pCar = info.GetEntity(); 
	replayAssert(pCar->GetVehicleType() == VEHICLE_TYPE_CAR);
	// Extract base packet.
	CPacketVehicleUpdate::Extract(info);
	
	if(pCar->GetVehicleAudioEntity())
	{			
		pCar->GetVehicleAudioEntity()->SetReplayRevLimiter(m_bOnRevLimiter);
	}
}

#endif // GTA_REPLAY
