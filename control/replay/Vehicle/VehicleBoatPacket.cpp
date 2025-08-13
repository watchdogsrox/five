#include "VehicleBoatPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "audio/boataudioentity.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "vehicles/Boat.h"

//////////////////////////////////////////////////////////////////////////
void CPacketBoatUpdate::Store(CBoat *pBoat, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketBoatUpdate);
	replayFatalAssertf(pBoat->GetVehicleType() == VEHICLE_TYPE_BOAT, "Incorrect vehicle type in CPacketBoatUpdate::Store");

	CPacketVehicleUpdate::Store(PACKETID_BOATUPDATE, sizeof(CPacketBoatUpdate), pBoat, storeCreateInfo);

	m_rudderAngle = pBoat->m_BoatHandling.GetRudderAngle();
	m_propDeformationOffset1 = pBoat->m_BoatHandling.GetPropDeformOffset1();
	m_propDeformationOffset2 = pBoat->m_BoatHandling.GetPropDeformOffset2();

	//Audio data
	m_isInWater = false;
	audBoatAudioEntity* pAudioEntity= ((audBoatAudioEntity*)pBoat->GetVehicleAudioEntity());
	if(pAudioEntity)
	{
		m_isInWater = pAudioEntity->CalculateIsInWater();
	}
	
}


void CPacketBoatUpdate::StoreExtensions(CBoat *pBoat, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketBoatUpdate);
	replayFatalAssertf(pBoat->GetVehicleType() == VEHICLE_TYPE_BOAT, "Incorrect vehicle type in CPacketBoatUpdate::Store");
	CPacketVehicleUpdate::StoreExtensions(PACKETID_BOATUPDATE, sizeof(CPacketBoatUpdate), pBoat, storeCreateInfo);
}

//////////////////////////////////////////////////////////////////////////
void CPacketBoatUpdate::Extract(const CInterpEventInfo<CPacketBoatUpdate, CBoat>& info) const
{
	CBoat* pBoat = info.GetEntity();
	replayFatalAssertf(pBoat->GetVehicleType() == VEHICLE_TYPE_BOAT, "Incorrect vehicle type in CPacketBoatUpdate::Extract");

	pBoat->m_BoatHandling.SetRudderAngle(m_rudderAngle);
	pBoat->m_BoatHandling.SetPropDeformOffset1(m_propDeformationOffset1);
	pBoat->m_BoatHandling.SetPropDeformOffset2(m_propDeformationOffset2);

	//Audio data
	audBoatAudioEntity* pAudioEntity= ((audBoatAudioEntity*)pBoat->GetVehicleAudioEntity());
	if(pAudioEntity)
	{
		pAudioEntity->SetIsInWater(m_isInWater);
	}

	CPacketVehicleUpdate::Extract(info);
}

#endif // GTA_REPLAY