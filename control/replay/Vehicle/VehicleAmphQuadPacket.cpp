#include "VehicleAmphQuadPacket.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "game/modelindices.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "audio/caraudioentity.h"

//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketAmphQuadUpdate_V1 = 1;
void CPacketAmphQuadUpdate::Store(CAmphibiousQuadBike *pQuadbike, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketAmphQuadUpdate);
	replayAssert(pQuadbike->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE);
	CPacketVehicleUpdate::Store(PACKETID_AMPHQUADUPDATE, sizeof(CPacketAmphQuadUpdate), pQuadbike ,storeCreateInfo, CPacketAmphQuadUpdate_V1);

	//Audio data
	m_isInWater = false;
	m_IsInAmphibiousBoatMode = false;
	audCarAudioEntity* pAudioEntity= ((audCarAudioEntity*)pQuadbike->GetVehicleAudioEntity());
	if(pAudioEntity)
	{
		m_isInWater = pAudioEntity->CalculateIsInWater();
		m_IsInAmphibiousBoatMode = pAudioEntity->IsInAmphibiousBoatMode();
	}
}


void CPacketAmphQuadUpdate::StoreExtensions(CAmphibiousQuadBike *pQuadbike, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketAmphQuadUpdate);
	replayAssert(pQuadbike->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_AMPHQUADUPDATE, sizeof(CPacketAmphQuadUpdate), pQuadbike ,storeCreateInfo);
}

//////////////////////////////////////////////////////////////////////////
void CPacketAmphQuadUpdate::Extract(const CInterpEventInfo<CPacketAmphQuadUpdate, CAmphibiousQuadBike>& info) const
{
	CAmphibiousQuadBike* pVeh = info.GetEntity();
	replayAssert(pVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE);
	
	if(GetPacketVersion() >= CPacketAmphQuadUpdate_V1)
	{
		//Audio data
		audCarAudioEntity* pAudioEntity= ((audCarAudioEntity*)pVeh->GetVehicleAudioEntity());
		if(pAudioEntity)
		{
			pAudioEntity->SetIsInWater(m_isInWater);
			pAudioEntity->SetInAmphibiousBoatMode(m_IsInAmphibiousBoatMode);
		}
	}

	CPacketVehicleUpdate::Extract(info);
}



#endif // GTA_REPLAY